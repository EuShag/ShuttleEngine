#pragma once
#include <cassert>
#include <coroutine>
#include <atomic>
#include <expected>
#include <concepts>
#include <optional>
#include <type_traits>
#include <async/async_shared_state.hpp>
#include <async/error.hpp>
#include <async/mi_allocator.hpp>
#include <async/coro_scope.hpp>
#include <async/awaiters.hpp>

namespace shuttle::async {

    struct CancellationPoint {};
    inline constexpr CancellationPoint cancellationPoint{};

    template <typename T>
    class CancellationToken;

    class CoroScope;

    template <typename T, typename Promise, bool = std::same_as<T, void>>
    struct AsyncTaskReturnOps;

    template <typename T, typename Promise>
    struct AsyncTaskReturnOps<T, Promise, false> {
        void return_value(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
            auto& promise = static_cast<Promise&>(*this);
            ::new (promise.m_sharedState.getBufPtr()) T(std::move(value));
            promise.m_sharedState.signalReady();
            if (promise.m_coroScope) [[likely]] promise.m_coroScope->joinTask();
        }

        void return_value(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
            requires std::copy_constructible<T> {
            auto& promise = static_cast<Promise&>(*this);
            ::new (promise.m_sharedState.getBufPtr()) T(value);
            promise.m_sharedState.signalReady();
            if (promise.m_coroScope) [[likely]] promise.m_coroScope->joinTask();
        }
    };

    template <typename Promise>
    struct AsyncTaskReturnOps<void, Promise, true> {
        void return_void() noexcept {
            auto& promise = static_cast<Promise&>(*this);
            promise.m_sharedState.signalReady();
            if (promise.m_coroScope) [[likely]] promise.m_coroScope->joinTask();
        }
    };

    template <typename T = void>
    class [[nodiscard]] AsyncTask final {
    public:
        struct promise_type : AsyncTaskReturnOps<T, promise_type> {
            std::atomic<bool> m_isCanceled{ false };
            std::atomic<bool> m_started{ false };
            CoroScope* m_coroScope{ nullptr };
            AsyncSharedState<T> m_sharedState;

            template <typename TObj, typename... TArgs>
            promise_type(TObj&&, CoroScope& scope, TArgs&...) noexcept
                : m_started(scope.canImmediateLaunched()), m_coroScope(&scope) {}

            template <typename... TArgs>
            promise_type(CoroScope& scope, TArgs&...) noexcept
                : m_started(scope.canImmediateLaunched()), m_coroScope(&scope) {}

            promise_type() noexcept = default;

            [[nodiscard]] AsyncTask get_return_object() const noexcept {
                auto coroHandle = std::coroutine_handle<promise_type>::from_promise(*const_cast<promise_type*>(this));
                if (m_coroScope) [[likely]] {
                    m_coroScope->registerCoroHandle(coroHandle);
                }
                return AsyncTask{ AsyncFuture<T>{&const_cast<promise_type*>(this)->m_sharedState}, coroHandle };
            }

            [[nodiscard]] static AsyncTask get_return_object_on_allocation_failure() noexcept {
                return AsyncTask{ Error::allocation_failure };
            }

            [[nodiscard]] auto initial_suspend() const noexcept { return InitialAwaiter{ m_coroScope }; }
            [[nodiscard]] auto final_suspend() const noexcept { return FinalAwaiter{ m_coroScope }; }

            void unhandled_exception() const noexcept { std::terminate(); }

            void cancel() noexcept { m_isCanceled.store(true, std::memory_order_release); }
            [[nodiscard]] bool isCanceled() const noexcept { return m_isCanceled.load(std::memory_order_acquire); }

            Status start(std::coroutine_handle<> handle, IExecutor* executor) noexcept {
                bool expected = false;
                if (!m_started.compare_exchange_strong(
                        expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return {};
                }
                if (executor == nullptr) [[unlikely]] {
                    m_started.store(false, std::memory_order_release);
                    return std::unexpected(Error::executor_unavailable);
                }
                auto result = executor->execute(handle);
                if (!result) {
                    m_started.store(false, std::memory_order_release);
                }
                return result;
            }

            // --- ИНЖЕКЦИЯ ФЛАГА ОТМЕНЫ ЧЕРЕЗ AWAIT_TRANSFORM ---
            template <typename Awaitable>
                requires requires(Awaitable& awaitable, std::atomic<bool>* flag) {
                    awaitable.claimAwait();
                    awaitable.prepareAwaiter(flag);
                }
            [[nodiscard]] auto await_transform(Awaitable&& awaitable) noexcept {
                if (!awaitable.claimAwait()) [[unlikely]] {
                    std::terminate();
                }
                // Пробрасываем текущий экзекьютор из TLS и адрес локального атомика отмены m_isCanceled
                return awaitable.prepareAwaiter(&m_isCanceled);
            }

            template <typename U>
            [[nodiscard]] typename AsyncFuture<U>::FutureAwaiter await_transform(AsyncTask<U>& task) noexcept {
                return task.getAwaiter(IExecutor::getThreadPinnedExecutor(), &m_isCanceled);
            }

            template <typename U>
            [[nodiscard]] typename AsyncFuture<U>::FutureAwaiter await_transform(AsyncTask<U>&& task) noexcept {
                return task.getAwaiter(IExecutor::getThreadPinnedExecutor(), &m_isCanceled);
            }

            struct CancellationAwaiter {
                std::atomic<bool>* flag;
                [[nodiscard]] bool await_ready() const noexcept { return true; }
                void await_suspend(std::coroutine_handle<>) const noexcept {}
                [[nodiscard]] AsyncResult<void> await_resume() const noexcept {
                    return AsyncResult<void>{.isCancelled = flag->load(std::memory_order_acquire)};
                }
            };

            [[nodiscard]] CancellationAwaiter await_transform(CancellationPoint) noexcept {
                return CancellationAwaiter{&m_isCanceled};
            }

            template <typename Awaitable>
                requires (!std::same_as<std::remove_cvref_t<Awaitable>, CancellationPoint>)
                    && (!requires(std::remove_cvref_t<Awaitable>& awaitable, std::atomic<bool>* flag) {
                        awaitable.claimAwait();
                        awaitable.prepareAwaiter(flag);
                    })
            decltype(auto) await_transform(Awaitable&& awaitable) noexcept {
                return std::forward<Awaitable>(awaitable);
            }

            struct SwitchExecutorAwaiter final {
                IExecutor* targetExecutor{ nullptr };
                std::atomic<bool>* parentCancelledFlag{ nullptr }; // Неявный указатель на флаг отмены родителя

                [[nodiscard]] constexpr bool await_ready() const noexcept {
                    // Мы ВСЕГДА приостанавливаемся, чтобы честно выполнить
                    // маршаллинг задачи в очередь нового исполнителя
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> currentCoro) noexcept {
                    // FAST-PATH ЗАЩИТА: Если корутина отменена прямо перед прыжком —
                    // вообще никуда не летим, мгновенно просыпаемся на текущем месте для эвакуации!
                    if (parentCancelledFlag && parentCancelledFlag->load(std::memory_order_acquire)) {
                        return currentCoro;
                    }

                    if (targetExecutor) [[likely]] {
                        // Толкаем текущий хэндл в лок-фри воронку нового экзекьютора!
                        // Метод execute() сам разберется, пушить в деку или на Шаг 0 воркера.
                        if (!targetExecutor->execute(currentCoro)) [[unlikely]] {
                            return currentCoro;
                        }
                    } else {
                        return currentCoro;
                    }

                    // Текущий физический поток ОС полностью свободен, уходит в анабиоз или за задачами!
                    return IExecutor::getCurrentDispatcherCoroutine();
                }

                // =================================================================
                // ТОЧКА ПРОБОРКИ ОТМЕНЫ ПОСЛЕ ПЕРЕКЛЮЧЕНИЯ (Ваша идея!)
                // =================================================================
                [[nodiscard]] AsyncResult<void> await_resume() const noexcept {
                    bool const cancelled = parentCancelledFlag ? parentCancelledFlag->load(std::memory_order_acquire) : false;

                    // Возвращаем ультралегкий флаг. Если false — пользователь прервет выполнение.
                    return AsyncResult<void>{ .isCancelled = cancelled };
                }
            };

            // Перегрузка Б: Ультимативный Fast-Path (прямой co_await указателя экзекьютора!)
            // Позволяет писать экстремально лаконично: co_await IExecutor::getThreadPinnedExecutor();
            [[nodiscard]] auto await_transform(IExecutor* explicitExecutor) noexcept {
                return SwitchExecutorAwaiter{
                    .targetExecutor = explicitExecutor,
                    .parentCancelledFlag = &m_isCanceled
                };
            }

            // --- СТРОГАЯ SIZED-DEALLOCATION MIALOCATOR АЛЛОКАЦИЯ ---
            [[nodiscard]] void* operator new(std::size_t size) noexcept {
                return MiAllocator<char>{}.allocate(size);
            }
            void operator delete(void* ptr, std::size_t size) noexcept {
                MiAllocator<char>{}.deallocate(static_cast<char*>(ptr));
            }
        };

        AsyncTask(AsyncFuture<T> future, std::coroutine_handle<promise_type> handle) noexcept
            : m_future{std::move(future)}, m_handle{handle} {}
        AsyncTask(Error error) noexcept
            : m_future{}, m_handle{nullptr}, m_error{error} {}

        // =================================================================
        // ВАШЕ НОВОЕ ПРАВИЛО: АВТОМАТИЧЕСКАЯ ОТМЕНА В ДЕСТРУКТОРЕ (RAII)
        // =================================================================
        ~AsyncTask() noexcept {
            if (m_handle) {
                // Если дескриптор задачи уничтожается (выходит из области видимости,
                // сбрасывается или отменяется родителем) — мы принудительно гасим задачу!
                m_handle.promise().cancel();

                // ВАЖНО: Сами фрейм мы НЕ уничтожаем! Управлением жизни фрейма
                // монолитно рулит CoroScope и JointerTask строго на родном воркере!
            }
        }

        [[nodiscard]] CancellationToken<T> cancel() noexcept;

        [[nodiscard]] bool isCanceled() const noexcept {
            return m_handle ? m_handle.promise().isCanceled() : true;
        }

        // Move-only контракт
        AsyncTask(const AsyncTask&) = delete;
        AsyncTask& operator=(const AsyncTask&) = delete;

        AsyncTask(AsyncTask&& other) noexcept
            : m_future(std::move(other.m_future)),
              m_handle(std::exchange(other.m_handle, nullptr)),
              m_error{std::move(other.m_error)} {
            other.m_error.reset();
        }

        AsyncTask& operator=(AsyncTask&& other) noexcept {
            if (this != &other) {
                m_future = std::move(other.m_future);
                m_handle = std::exchange(other.m_handle, nullptr);
                m_error = std::move(other.m_error);
                other.m_error.reset();
            }
            return *this;
        }

        // Перегрузка 1: Явное указание Executor для возврата "домой"
        // Использование: co_await task.get(IExecutor::getMainThreadExecutor());
        [[nodiscard]] typename AsyncFuture<T>::FutureAwaitable get(IExecutor* explicitExecutor) noexcept {
            requireValid();
            startForAwait();
            if (explicitExecutor == nullptr && !m_future.isSignaled()) [[unlikely]] {
                std::terminate();
            }
            // Мы бережно передаем хэндл в авейтер, обнуляя его у себя (std::exchange).
            // Наш деструктор ~AsyncTask() увидит nullptr и НЕ вызовет ложную отмену!
            std::exchange(m_handle, nullptr);
            return m_future.prepareAwaitable(explicitExecutor);
        }

        // Перегрузка 2: Экзекьютор по умолчанию (ThreadBind из TLS текущего потока)
        // Использование: co_await task.get();
        [[nodiscard]] typename AsyncFuture<T>::FutureAwaitable get() noexcept {
            requireValid();
            startForAwait();
            IExecutor* const executor = IExecutor::getThreadPinnedExecutor();
            if (executor == nullptr && !m_future.isSignaled()) [[unlikely]] {
                std::terminate();
            }
            std::exchange(m_handle, nullptr);
            return m_future.prepareAwaitable(executor);
        }

        [[nodiscard]] typename AsyncFuture<T>::FutureAwaiter getAwaiter(
            IExecutor* executor,
            std::atomic<bool>* cancellationFlag) noexcept {
            requireValid();
            startForAwait();
            if (executor == nullptr && !m_future.isSignaled()) [[unlikely]] {
                std::terminate();
            }
            if (!m_future.claimConsumer()) [[unlikely]] {
                std::terminate();
            }
            std::exchange(m_handle, nullptr);
            return m_future.prepareAwaitable(executor).prepareAwaiter(cancellationFlag);
        }

        // Дефолтный коавейт самого AsyncTask (если программист опустил вызов .get())
        auto operator co_await() noexcept {
            return get(); // Просто перенаправляем в дефолтный TaskAwaiter
        }

        Status start(IExecutor* executor) noexcept {
            if (!m_handle || m_handle == std::noop_coroutine()) [[unlikely]] {
                return std::unexpected(m_error.value_or(Error::invalid_coroutine_handle));
            }
            return m_handle.promise().start(m_handle, executor);
        }

        Status start() noexcept {
            return start(IExecutor::getThreadPinnedExecutor());
        }

        [[nodiscard]] bool has_value() const noexcept { return m_handle != nullptr && !m_error.has_value(); }
        std::coroutine_handle<> get_handle() noexcept {
            assert(has_value());
            return m_handle;
        }
        [[nodiscard]] std::optional<Error> getError() const noexcept { return m_error; }

    private:
        template <typename>
        friend class BlockingTask;
        template <typename>
        friend class CancellationToken;

        void requireValid() const noexcept {
            if (!has_value()) [[unlikely]] {
                std::terminate();
            }
        }

        void requireSingleConsumer() noexcept {
            if (!m_future.claimConsumer()) [[unlikely]] {
                std::terminate();
            }
        }

        void startForAwait() noexcept {
            if (m_handle.promise().m_started.load(std::memory_order_acquire)) {
                return;
            }
            auto result = start(IExecutor::getThreadPinnedExecutor());
            if (!result) {
                m_handle.promise().m_started.store(true, std::memory_order_release);
                m_handle.resume();
            }
        }

        AsyncFuture<T> m_future{};
        std::coroutine_handle<promise_type> m_handle{ nullptr };
        std::optional<Error> m_error;
    };

    template <typename T>
    class [[nodiscard]] CancellationToken final {
    public:
        explicit CancellationToken(AsyncTask<T>&& task) noexcept
            : m_task{std::move(task)} {}

        CancellationToken(const CancellationToken&) = delete;
        CancellationToken& operator=(const CancellationToken&) = delete;
        CancellationToken(CancellationToken&&) noexcept = default;
        CancellationToken& operator=(CancellationToken&&) noexcept = default;

        [[nodiscard]] bool has_value() const noexcept {
            return m_task.has_value();
        }

        [[nodiscard]] std::optional<Error> getError() const noexcept {
            return m_task.getError();
        }

        struct Awaiter {
            typename AsyncFuture<T>::FutureAwaiter m_taskAwaiter;

            [[nodiscard]] bool await_ready() const noexcept {
                return m_taskAwaiter.await_ready();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) noexcept {
                return m_taskAwaiter.await_suspend(handle);
            }

            [[nodiscard]] auto await_resume() noexcept {
                auto result = m_taskAwaiter.await_resume();
                result.isCancelled = true;
                return result;
            }
        };

        [[nodiscard]] Awaiter get(IExecutor* resumeExecutor = IExecutor::getThreadPinnedExecutor()) noexcept {
            return Awaiter{m_task.getAwaiter(resumeExecutor, nullptr)};
        }

        Awaiter operator co_await() & noexcept {
            return get();
        }

        Awaiter operator co_await() && noexcept {
            return operator co_await();
        }

    private:
        AsyncTask<T> m_task;
    };

    template <typename T>
    CancellationToken<T> AsyncTask<T>::cancel() noexcept {
        if (m_handle) [[likely]] {
            m_handle.promise().cancel();
        }
        return CancellationToken<T>{std::move(*this)};
    }

    template <typename T>
    class BlockingTask {
        BlockingExecutor m_executor{};
    public:
        BlockingTask(AsyncTask<T>&& task) noexcept : m_task{std::move(task)} {}

        Status start(IExecutor* executor) noexcept {
            return m_task.start(executor);
        }
        Status start() noexcept {
            return m_task.start();
        }

        [[nodiscard]] CancellationToken<T> cancel() noexcept {
            return m_task.cancel();
        }

        [[nodiscard]] bool has_value() const noexcept {
            return m_task.has_value();
        }

        [[nodiscard]] std::optional<Error> getError() const noexcept {
            return m_task.getError();
        }

        decltype(auto) get() {
            m_task.requireValid();
            m_task.requireSingleConsumer();
            m_task.startForAwait();
            auto threadContextGuard = m_executor.initializeThreadContext();
            // 1. Получаем awaitable, передавая адрес нашего встроенного m_executor.
            // Напрямую лезем во внутренний стейт AsyncTask.
            auto awaitable = m_task.m_future.prepareAwaitable(&m_executor);
            auto* state = awaitable.m_state;

            // -----------------------------------------------------------------
            // FAST-PATH ЭТАП 1: Данные УЖЕ готовы (Продюсер успел до вызова get)
            // -----------------------------------------------------------------
            if (state->isSignaled()) {
                if constexpr (std::same_as<T, void>) return;
                else return state->extractValue();
            }

            // -----------------------------------------------------------------
            // FAST-PATH ЭТАП 2: Гонка с продюсером в момент регистрации.
            // Если tryRegisterWaiter вернул false, значит пока мы записывали
            // указатели, продюсер СРАЗУ переключил стейт в Signaled!
            // -----------------------------------------------------------------
            if (!state->tryRegisterWaiter(std::noop_coroutine(), &m_executor)) {
                if constexpr (std::same_as<T, void>) return;
                else return state->extractValue();
            }

            // -----------------------------------------------------------------
            // SLOW-PATH ЭТАП 3: Данных нет, ждун успешно зарегистрирован.
            // Уводим физический поток ОС в жесткий анабиоз ядра на фьютексе/WaitOnAddress.
            // Защита от ложных пробуждений (while-цикл) уже внутри waitForSignal!
            // -----------------------------------------------------------------
            m_executor.waitForSignal();

            // Поток проснулся, данные гарантированно на месте.
            // Извлекаем r-value ссылку без единого копирования!
            if constexpr (std::same_as<T, void>) return;
            else return state->extractValue();
        }

    private:
        AsyncTask<T> m_task;
    };

} // namespace shuttle::async
