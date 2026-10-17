#pragma once
#include <coroutine>
#include <atomic>
#include <cstddef>
#include <exception>
#include <utility>
#include <new>
#include <type_traits>
#include <async/i_executor.hpp>
#include <async/error.hpp>
#include <async/async_result.hpp>

namespace shuttle::async {

    // Перечисление состояний жизненного цикла задачи
    enum class AsyncTaskState : uint32_t {
        Pending   = 0, // Ожидание результата
        Suspended = 1, // Корутина-ждун приостановлена
        Signaled  = 2  // Данные готовы
    };

    // Опережающие объявления
    template <typename T> class AsyncFuture;
    template <typename T> class AsyncPromise;

    template <typename T = void>
    class AsyncSharedState final {
    public:
        AsyncSharedState() noexcept
            : m_state(AsyncTaskState::Pending), m_handle(nullptr), m_executor(nullptr) {}

        ~AsyncSharedState() noexcept {
            if constexpr (!std::same_as<T, void>) {
                if (isSignaled()) {
                    reinterpret_cast<T*>(m_valueBuf)->~T();
                }
            }
        }

        AsyncSharedState(const AsyncSharedState&) = delete;
        AsyncSharedState& operator=(const AsyncSharedState&) = delete;

        [[nodiscard]] bool isSignaled() const noexcept {
            return m_state.load(std::memory_order_acquire) == AsyncTaskState::Signaled;
        }

        [[nodiscard]] bool claimConsumer() noexcept {
            bool expected = false;
            return m_consumerClaimed.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel, std::memory_order_relaxed);
        }

        void* getBufPtr() noexcept { return static_cast<void*>(m_valueBuf); }

        [[nodiscard]] bool tryRegisterWaiter(std::coroutine_handle<> handle, IExecutor* executor) noexcept {
            // 1. Записываем поля. Нам не важен их порядок между собой.
            m_handle = handle;
            m_executor = executor;

            // 2. КРИТИЧЕСКИЙ БАРЬЕР: СТРОГИЙ std::memory_order_release!
            // Он аппаратно гарантирует процессору: записи m_handle и m_executor
            // ЖЕЛЕЗОБЕТОННО зафиксированы в кэше и НЕ могут быть перенесены ниже этой строчки!
            AsyncTaskState expected = AsyncTaskState::Pending;
            if (m_state.compare_exchange_strong(expected, AsyncTaskState::Suspended, std::memory_order_release, std::memory_order_relaxed)) {
                return true; // Успешно ушли в сон, продюсер нас подхватит во втором CAS
            }

            // Если CAS вернул false, значит состояние УЖЕ Signaled (Продюсер успел раньше)
            return false;
        }

        void signalReady() noexcept {
            // Первый CAS: Пытаемся закрыть сделку на фазе Pending.
            // Используем release, чтобы гарантировать видимость данных (m_valueBuf) для Ждуна.
            AsyncTaskState expected = AsyncTaskState::Pending;
            if (m_state.compare_exchange_strong(expected, AsyncTaskState::Signaled, std::memory_order_release, std::memory_order_relaxed)) {
                return; // Ждун еще не пришел, мы первыми записали результат
            }

            // Второй CAS: Ждун успел выставить Suspended.
            // ВАЖНО: Используем std::memory_order_acquire!
            // Он синхронизируется с memory_order_release Ждуна и гарантирует, что к моменту
            // прохода этой строчки поля m_executor и m_handle УЖЕ полностью долетели до нашего ядра CPU!
            expected = AsyncTaskState::Suspended;
            if (m_state.compare_exchange_strong(expected, AsyncTaskState::Signaled, std::memory_order_acquire, std::memory_order_relaxed)) {
                if (m_executor && m_handle) [[likely]] {
                    auto resumeResult = m_executor->execute(m_handle);
                    if (!resumeResult) [[unlikely]] {
                        m_handle.resume();
                    }
                }
            }
        }

        // Возвращаем r-value ссылку на внутренний объект кадра!
        template <typename U = T>
        [[nodiscard]] U&& extractValue() noexcept requires (!std::same_as<U, void>) {
            return std::move(*reinterpret_cast<U*>(m_valueBuf));
        }

    private:
        using ValueStorage = std::conditional_t<std::same_as<T, void>, std::max_align_t, T>;

        std::atomic<bool> m_consumerClaimed{false};
        std::atomic<AsyncTaskState> m_state;
        std::coroutine_handle<> m_handle;
        IExecutor* m_executor;
        alignas(ValueStorage) char m_valueBuf[std::same_as<T, void> ? 1 : sizeof(ValueStorage)]{};
    };

    /**
     * --- ПОТРЕБИТЕЛЬ БУДУЩЕГО: AsyncFuture<T> ---
     */
    template <typename T>
    class [[nodiscard]] AsyncFuture final {
    public:
        explicit AsyncFuture(AsyncSharedState<T>* state) noexcept : sharedState(state) {}
        AsyncFuture() = default;

        struct FutureAwaiter {
            AsyncSharedState<T>* m_state;
            IExecutor* m_executor{};
            std::atomic<bool>* parentCancelledFlag{ nullptr };

            [[nodiscard]] bool await_ready() const noexcept { return m_state ? m_state->isSignaled() : true; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
                if (parentCancelledFlag && parentCancelledFlag->load(std::memory_order_acquire)) {
                    return h;
                }
                if (m_state && m_state->tryRegisterWaiter(h, m_executor)) {
                    return IExecutor::getCurrentDispatcherCoroutine();
                }
                return h;
            }

            // ВАШЕ НОВОЕ ПРАВИЛО: Возвращаем AsyncResult с R-Value ссылкой на объект!
            [[nodiscard]] auto await_resume() noexcept {
                bool const cancelled = parentCancelledFlag ? parentCancelledFlag->load(std::memory_order_acquire) : false;

                if constexpr (std::same_as<T, void>) {
                    return AsyncResult<void>{ .isCancelled = cancelled };
                } else {
                    if (cancelled) [[unlikely]] {
                        return AsyncResult<T>{ .value = T{}, .isCancelled = true }; // Безопасный дефолтный объект
                    }
                    return AsyncResult<T>{ .value = m_state->extractValue(), .isCancelled = false }; // Чистый move
                }
            }
        };

        struct FutureAwaitable {
            AsyncSharedState<T>* m_state;
            IExecutor* m_executor{};
            bool m_awaited{false};

            FutureAwaitable(AsyncSharedState<T>* state, IExecutor* executor) noexcept
                : m_state(state), m_executor(executor) {}
            FutureAwaitable(const FutureAwaitable&) = delete;
            FutureAwaitable& operator=(const FutureAwaitable&) = delete;
            FutureAwaitable(FutureAwaitable&&) noexcept = default;
            FutureAwaitable& operator=(FutureAwaitable&&) noexcept = default;

            [[nodiscard]] bool claimAwait() noexcept {
                if (m_awaited || !m_state || !m_state->claimConsumer()) {
                    return false;
                }
                m_awaited = true;
                return true;
            }

            FutureAwaiter prepareAwaiter(std::atomic<bool>* cancelFlag) noexcept {
                return FutureAwaiter{ .m_state = m_state, .m_executor = m_executor, .parentCancelledFlag = cancelFlag};
            }

            FutureAwaiter operator co_await() && noexcept {
                if (!claimAwait()) [[unlikely]] {
                    std::terminate();
                }
                return prepareAwaiter(nullptr);
            }
        };

        FutureAwaitable prepareAwaitable(IExecutor* exec) noexcept {
            return FutureAwaitable{sharedState, exec};
        }

        [[nodiscard]] bool isSignaled() const noexcept {
            return sharedState && sharedState->isSignaled();
        }

        [[nodiscard]] bool claimConsumer() noexcept {
            return sharedState && sharedState->claimConsumer();
        }

        // Контракт Move-Only для AsyncFuture<T>
        AsyncFuture(AsyncFuture const&) = delete;
        AsyncFuture& operator=(AsyncFuture const&) = delete;

        // 1. Конструктор перемещения (Move Constructor)
        // Забираем адрес стейта у соседа, а его поле чисто зануляем за 0 тактов CPU!
        AsyncFuture(AsyncFuture&& other) noexcept
            : sharedState(std::exchange(other.sharedState, nullptr)) {}

        // 2. Оператор перемещения (Move Assignment)
        AsyncFuture& operator=(AsyncFuture&& other) noexcept {
            if (this != &other) [[likely]] {
                sharedState = std::exchange(other.sharedState, nullptr);
            }
            return *this;
        }

    private:
        AsyncSharedState<T>* sharedState{ nullptr };
    };

    /**
     * --- ПОСТАВЩИК ОБЕЩАНИЙ: AsyncPromise<T> ---
     */
    template <typename T>
    class AsyncPromise final {
    public:
        explicit AsyncPromise(AsyncSharedState<T>* state) noexcept : sharedState(state) {}

        void setValue(T&& value) noexcept requires (!std::same_as<T, void>) {
            if (sharedState) [[likely]] {
                ::new (sharedState->getBufPtr()) T(std::move(value));
                sharedState->signalReady();
            }
        }

        void setValue(const T& value) noexcept requires (!std::same_as<T, void> && std::copy_constructible<T>) {
            if (sharedState) [[likely]] {
                ::new (sharedState->getBufPtr()) T(value);
                sharedState->signalReady();
            }
        }

        void setValue() noexcept requires std::same_as<T, void> {
            if (sharedState) [[likely]] {
                sharedState->signalReady();
            }
        }

    private:
        AsyncSharedState<T>* sharedState{ nullptr };
    };

} // namespace shuttle::async
