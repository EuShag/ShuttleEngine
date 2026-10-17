//
// Created by Shagu on 12.10.2026.
//

#ifndef SHUTTLEENGINE_COROSCOPE_HPP
#define SHUTTLEENGINE_COROSCOPE_HPP
#include <atomic>
#include <cinttypes>
#include <concepts>
#include <coroutine>
#include <future>
#include <functional>
#include <type_traits>
#include <utility>
#include <thread>

#include <async/i_executor.hpp>
#include <async/error.hpp>
#include <async/launch_mode.hpp>
#include <async/mi_allocator.hpp>

namespace shuttle::async {

    template <typename T>
    class AsyncTask;

    template <typename T>
    class BlockingTask;

    class CoroScope;

    template <typename T>
    struct IsAsyncTask : std::false_type {};

    template <typename T>
    struct IsAsyncTask<AsyncTask<T>> : std::true_type {};

    template <typename F, typename... Args>
    concept CoroTaskFactory =
        std::invocable<F, CoroScope, Args...>
        && IsAsyncTask<std::remove_cvref_t<std::invoke_result_t<F, CoroScope, Args...>>>::value;

    // Старший бит — маркер финализации родителя
    inline constexpr uint64_t DEATH_SIGNAL = 1ULL << 63;

    class BlockingExecutor final : public IExecutor {
    public:
        Status execute(std::coroutine_handle<>) override {
            // Жесткий release: публикуем все данные корутины для ждущего потока
            m_ready.store(true, std::memory_order_release);
            m_ready.notify_all();
            return {};
        }

        void waitForSignal() noexcept {
            // Защита от ложных пробуждений (Spurious Wakeups)
            // Если проснулись случайно, а флаг все еще false — уходим спать дальше
            while (!m_ready.load(std::memory_order_acquire)) {
                // Передаем false: засыпаем ТОЛЬКО если m_ready равен false.
                // Если он уже true, wait() мгновенно вернет управление без сна.
                m_ready.wait(false, std::memory_order_acquire);
            }

            // Сбрасываем в false для следующих итераций (например, для joinTask в деструкторе)
            m_ready.store(false, std::memory_order_relaxed);
        }

        ThreadContextGuard initializeThreadContext() noexcept {
            return ThreadContextGuard{{
                .m_sharedExecutor = this,
                .m_threadPinnedExecutor = this,
                .m_dispatcherCoroHandle = std::noop_coroutine()
            }};
        }

    private:
        std::atomic<bool> m_ready{false};
    };

    class JointerTask {
    public:
        struct Initial{
            [[nodiscard]] bool await_ready() const noexcept { return false; }
            [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroHandle) const noexcept;
            void await_resume() const noexcept {}
            CoroScope* m_scopePtr;
        };

        struct promise_type {
            auto get_return_object() noexcept {
                return JointerTask{};
            }

            promise_type(CoroScope& scope) {
                m_scopePtr = &scope;
            }

            [[nodiscard]] Initial initial_suspend() const noexcept { return Initial{m_scopePtr};}
            [[nodiscard]] std::suspend_never final_suspend() const noexcept { return {}; }
            void unhandled_exception() noexcept { std::terminate(); }

            void return_void() const noexcept {}

            void* operator new(std::size_t size) noexcept {
                return MiAllocator<char>{}.allocate(size);
            }
            void operator delete(void* ptr) noexcept {
                return MiAllocator<char>{}.deallocate(static_cast<char *>(ptr));
            }

            CoroScope* m_scopePtr;
        };
    };

    /**
     * --- УЛЬТРАЛЕГКИЙ COROSCOPE (MOVE-BY-VALUE ТОКЕН) ---
     * Передается в корутины по значению. Физически удерживается внутри фрейма параметров.
     */
    class CoroScope {
    public:

        // Деструктор скоупа пуст: он сработает строго на фазе .destroy() фрейма!
        ~CoroScope() {
            if (m_parentScope) {
                decrementChildCount();
            }
        };

        // Жесткий Move-Only контракт токена
        CoroScope(const CoroScope&) = delete;
        CoroScope& operator=(const CoroScope&) = delete;// ПРАВИЛЬНЫЙ КОНСТРУКТОР ПЕРЕМЕЩЕНИЯ ДЛЯ ПЕРЕНОСА ВО ФРЕЙМ
        CoroScope(CoroScope&& other) noexcept
            : m_launch_options(other.m_launch_options),
              m_childCount(other.m_childCount.load(std::memory_order_relaxed)),
              m_threadExecutor(other.m_threadExecutor),
              m_ownerHandle(other.m_ownerHandle.load(std::memory_order_relaxed)),
              m_jointerTaskHandle(other.m_jointerTaskHandle.load(std::memory_order_relaxed)),
              m_parentScope(other.m_parentScope)
        {
            // Очищаем старый временный объект на стеке,
            // чтобы его деструктор НЕ сделал decrementChildCount() родителя!
            other.m_parentScope = nullptr;
            other.m_childCount.store(0, std::memory_order_relaxed);
            other.m_ownerHandle.store(nullptr, std::memory_order_relaxed);
        }

        // Оператор присваивания нам не нужен для параметров, его можно с чистой совестью снести
        CoroScope& operator=(CoroScope&& other) = delete;

        /**
         * --- МЕТОД CREATE_CHILD ---
         * Рождает НОВЫЙ независимый объект скоупа по значению, привязывая гард к ТЕКУЩЕМУ скоупу.
         */
        CoroScope createChild(LaunchOptions options) noexcept;

        /**
         * --- ДЕКЛАРАТИВНЫЙ LAUNCH ---
         * Вызывает фабрику корутины, передавая дочерний CoroScope по значению!
         */
        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        auto launch(F&& func, LaunchOptions launchOptions, Args&&... args) noexcept {
            auto coro = std::invoke(
                std::forward<F>(func),
                this->createChild(launchOptions),
                std::forward<Args>(args)...
            );
            if (!coro.has_value()) [[unlikely]] {
                return coro;
            }

            if (launchOptions.mode == LaunchMode::Async) {
                auto childHandle = coro.get_handle();
                if (launchOptions.explicitExecutor == nullptr) [[unlikely]] {
                    childHandle.destroy();
                    return decltype(coro){Error::executor_unavailable};
                }

                auto queueResult = coro.start(launchOptions.explicitExecutor);
                if (!queueResult) [[unlikely]] {
                    childHandle.destroy();
                    return decltype(coro){queueResult.error()};
                }
            }

            return coro;
        }

        template<typename F>
            requires CoroTaskFactory<F>
        auto launch(F&& func) noexcept {
            return launch(std::forward<F>(func), m_launch_options);
        }

        template<typename F, typename FirstArg, typename... Args>
            requires (!std::same_as<std::remove_cvref_t<FirstArg>, LaunchOptions>)
                && CoroTaskFactory<F, FirstArg, Args...>
        auto launch(F&& func, FirstArg&& firstArg, Args&&... args) noexcept {
            return launch(
                std::forward<F>(func),
                m_launch_options,
                std::forward<FirstArg>(firstArg),
                std::forward<Args>(args)...
            );
        }

        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launchImmediate(F&& func, Args&&... args) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Immediate, .explicitExecutor = nullptr}, std::forward<Args>(args)...);
        }
        template<typename F>
            requires CoroTaskFactory<F>
        [[nodiscard]] auto launchAsync(F&& func, IExecutor* executor = IExecutor::getThreadPinnedExecutor()) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Async, .explicitExecutor = executor});
        }
        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launchAsync(F&& func, IExecutor* executor, Args&&... args) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Async, .explicitExecutor = executor}, std::forward<Args>(args)...);
        }
        template<typename F, typename FirstArg, typename... Args>
            requires (!std::convertible_to<FirstArg, IExecutor*>)
                && CoroTaskFactory<F, FirstArg, Args...>
        [[nodiscard]] auto launchAsync(F&& func, FirstArg&& firstArg, Args&&... args) noexcept {
            return launch(
                std::forward<F>(func),
                {.mode = LaunchMode::Async, .explicitExecutor = IExecutor::getThreadPinnedExecutor()},
                std::forward<FirstArg>(firstArg),
                std::forward<Args>(args)...
            );
        }
        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launchLazy(F&& func, Args&&... args) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Lazy, .explicitExecutor = nullptr}, std::forward<Args>(args)...);
        }

        [[nodiscard]] bool canImmediateLaunched() const noexcept;

        [[nodiscard]] std::coroutine_handle<> getTransferCoro(std::coroutine_handle<> coroutineHandle) const noexcept;

        std::coroutine_handle<> checkCanDestroyed(std::coroutine_handle<> coroHandle) noexcept;

        // Атомарная лок-фри математика, инкапсулированная внутри скоупа

        void registerCoroHandle(std::coroutine_handle<> coroHandle) noexcept;
        [[nodiscard]] std::coroutine_handle<> getCoroHandle() const noexcept;
        void registerJointerTaskHandle(std::coroutine_handle<> coroHandle, IExecutor *ctx) noexcept;
        JointerTask joinTask() noexcept { co_return; }

    protected:

        explicit CoroScope(LaunchOptions options, CoroScope* parentScope = {}) noexcept
            : m_launch_options(options), m_parentScope(parentScope) {
            if (m_parentScope) {
                m_parentScope->incrementChildCount();
            }
        }

    private:
        LaunchOptions m_launch_options { .mode = LaunchMode::Async, .explicitExecutor = nullptr };
        // Единый регистр: старший бит — смерть, младшие 63 бита — счетчик живых детей
        std::atomic<uint64_t> m_childCount{ 0 };
        IExecutor* m_threadExecutor{};

        void incrementChildCount() noexcept;
        void decrementChildCount() noexcept;

        // Адрес публикации фрейма корутины-владельца этого скоупа
        std::atomic<void*> m_ownerHandle{ nullptr };
        std::atomic<void*> m_jointerTaskHandle{ nullptr };

        CoroScope* m_parentScope{ nullptr };
    };

    class JoinableScope : CoroScope{
    public:
        JoinableScope() : CoroScope {{.mode = LaunchMode::Async, .explicitExecutor = nullptr}, nullptr} {}

        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launch(F&& f, LaunchOptions options, Args&&... args) noexcept {
            return BlockingTask{std::move(CoroScope::launch(std::forward<F>(f), options, std::forward<Args>(args)...))};
        }

        template<typename F>
            requires CoroTaskFactory<F>
        auto launch(F&& f) noexcept {
            return CoroScope::launch(std::forward<F>(f));
        }

        template<typename F, typename FirstArg, typename... Args>
            requires (!std::same_as<std::remove_cvref_t<FirstArg>, LaunchOptions>)
                && CoroTaskFactory<F, FirstArg, Args...>
        auto launch(F&& f, FirstArg&& firstArg, Args&&... args) noexcept {
            return CoroScope::launch(
                std::forward<F>(f),
                std::forward<FirstArg>(firstArg),
                std::forward<Args>(args)...
            );
        }

        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launchImmediate(F&& func, Args&&... args) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Immediate, .explicitExecutor = nullptr}, std::forward<Args>(args)...);
        }
        template<typename F>
            requires CoroTaskFactory<F>
        [[nodiscard]] auto launchAsync(F&& func, IExecutor* executor = IExecutor::getThreadPinnedExecutor()) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Async, .explicitExecutor = executor});
        }
        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launchAsync(F&& func, IExecutor* executor, Args&&... args) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Async, .explicitExecutor = executor}, std::forward<Args>(args)...);
        }
        template<typename F, typename FirstArg, typename... Args>
            requires (!std::convertible_to<FirstArg, IExecutor*>)
                && CoroTaskFactory<F, FirstArg, Args...>
        [[nodiscard]] auto launchAsync(F&& func, FirstArg&& firstArg, Args&&... args) noexcept {
            return launch(
                std::forward<F>(func),
                {.mode = LaunchMode::Async, .explicitExecutor = IExecutor::getThreadPinnedExecutor()},
                std::forward<FirstArg>(firstArg),
                std::forward<Args>(args)...
            );
        }
        template<typename F, typename... Args>
            requires CoroTaskFactory<F, Args...>
        [[nodiscard]] auto launchLazy(F&& func, Args&&... args) noexcept {
            return launch(std::forward<F>(func), {.mode = LaunchMode::Lazy, .explicitExecutor = nullptr}, std::forward<Args>(args)...);
        }

        void joinAll() {
            auto threadContextGuard = m_blockingExecutor.initializeThreadContext();
            registerJointerTaskHandle(std::noop_coroutine(), &m_blockingExecutor);
            if (checkCanDestroyed(std::noop_coroutine()) == nullptr) {
                m_blockingExecutor.waitForSignal();
            }
        }

        ~JoinableScope() {
            joinAll();
        }
    private:
        BlockingExecutor m_blockingExecutor;
    };

} // namespace shuttle::async

#include <async/async_task.hpp>

#endif //SHUTTLEENGINE_COROSCOPE_HPP
