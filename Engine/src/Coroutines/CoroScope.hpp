//
// Created by Shagu on 12.10.2026.
//

#ifndef SHUTTLEENGINE_COROSCOPE_HPP
#define SHUTTLEENGINE_COROSCOPE_HPP
#include <atomic>
#include <cinttypes>
#include <coroutine>
#include <future>
#include <utility>
#include <thread>

#include "BlockingTask.hpp"
#include "IExecuter.hpp"
#include "LaunchMode.hpp"
#include "ResultCode.hpp"

namespace shuttle::engine {

    // Старший бит — маркер финализации родителя
    inline constexpr uint64_t DEATH_SIGNAL = 1ULL << 63;

    class JointerTask {
    public:
        struct Initial{
            [[nodiscard]] bool await_ready() const noexcept {return false;}
            [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroHandle) const noexcept;
            void await_resume() const noexcept {}
            CoroScope* m_scopePtr;
        };
        struct Final {
            [[nodiscard]] bool await_ready() const noexcept {return false;}
            [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                return std::noop_coroutine();
            };
            void await_resume() const noexcept {}
        };

        struct promise_type {
            auto get_return_object() noexcept {
                return JointerTask{};
            }

            promise_type(CoroScope& scope) {
                m_scopePtr = &scope;
            }

            [[nodiscard]] Initial initial_suspend() const noexcept { return Initial{m_scopePtr};}
            [[nodiscard]] Final final_suspend() const noexcept { return Final{}; }
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
            decrementChildCount();
        };

        // Жесткий Move-Only контракт токена
        CoroScope(const CoroScope&) = delete;
        CoroScope& operator=(const CoroScope&) = delete;

        CoroScope(CoroScope&& other) noexcept
            : m_launch_options(other.m_launch_options),
              m_childCount(other.m_childCount.load(std::memory_order_relaxed)),
              m_ownerHandle(other.m_ownerHandle.load(std::memory_order_relaxed)),
              m_parentScope(other.m_parentScope) {}

        CoroScope& operator=(CoroScope&& other) noexcept {
            if (this != &other) {
                m_launch_options = other.m_launch_options;
                m_childCount.store(other.m_childCount.load(std::memory_order_relaxed), std::memory_order_relaxed);
                m_ownerHandle.store(other.m_ownerHandle.load(std::memory_order_relaxed), std::memory_order_relaxed);
                m_parentScope = other.m_parentScope;
            }
            return *this;
        }

        /**
         * --- МЕТОД CREATE_CHILD ---
         * Рождает НОВЫЙ независимый объект скоупа по значению, привязывая гард к ТЕКУЩЕМУ скоупу.
         */
        CoroScope createChild(LaunchOptions options) noexcept;

        /**
         * --- ДЕКЛАРАТИВНЫЙ LAUNCH ---
         * Вызывает фабрику корутины, передавая дочерний CoroScope по значению!
         */
        template<typename F>
        auto launch(F&& func, LaunchOptions launchOptions) noexcept {
            // 1. Вызываем фабрику дочки. Передаём КЛОН дочернего скоупа по значению.
            // Конструктор копирования/перемещения скоупа САМ автоматически
            // инкрементирует m_childCount текущего родителя! 0% ручной работы.
            auto coro = std::forward<F>(func)(this->createChild(launchOptions));
            if (!coro.has_value()) [[unlikely]] {
                return coro; // ОС Windows пуста, фрейм не создался
            }

            // 2. Обрабатываем исключительно инфраструктурный сбой пула
            if (launchOptions.mode == LaunchMode::Async) {
                auto childHandle = coro->get_handle();
                ResultCode const queueResult = launchOptions.explicitExecutor->execute(childHandle);

                if (is_error(queueResult)) [[unlikely]] {
                    // Пул перегружен (OVERFLOW очередей)
                    childHandle.destroy(); // Уничтожаем фрейм. Деструктор вшитого скоупа
                    // автоматически сделает decrementChildCount() родителя!
                    return decltype(coro)(std::unexpected(queueResult));
                }
            }

            // 3. УСПЕХ: Возвращаем объект задачи родительской корутине.
            // Никаких списков детей, никаких мьютексов регистрации, никаких аллокаций векторов!
            return coro;
        }
        
        template<typename F>
        auto launch(F&& func) noexcept {
            return launch(std::forward<F>(func), m_launch_options);
        }

        [[nodiscard]] bool canImmediateLaunched() const noexcept;

        [[nodiscard]] std::coroutine_handle<> getTransferCoro(std::coroutine_handle<> coroutineHandle) const noexcept;

        std::coroutine_handle<> checkCanDestroyed(std::coroutine_handle<> coroHandle) noexcept;

        // Атомарная лок-фри математика, инкапсулированная внутри скоупа

        void registerCoroHandle(std::coroutine_handle<> coroHandle) noexcept;
        [[nodiscard]] std::coroutine_handle<> getCoroHandle() const noexcept;
        void registerJointerTaskHandle(std::coroutine_handle<> coroHandle, IExecutor *ctx) noexcept;
        JointerTask joinTask() noexcept { co_return; }

        /**
         * СИНХРОННЫЙ БЛОКИРУЮЩИЙ ШЛЮЗ ДЛЯ MAIN.CPP
         */
        template<typename F>
        static void launchBlocking(F&& func, IExecutor* explicitExec = nullptr) noexcept {
            CoroScope rootScope({ .mode = LaunchMode::Async, .explicitExecutor = explicitExec });

            rootScope.launch([&func](CoroScope child, IExecutor* exec) mutable {
                auto userTask = std::forward<F>(func)(std::move(child), exec);
                if (userTask.has_value()) {
                    userTask->getHandle().resume();
                }
            }, { .mode = LaunchMode::Async, .explicitExecutor = explicitExec });

            while ((rootScope.m_childCount.load(std::memory_order_acquire) & ~DEATH_SIGNAL) > 0) {
                std::this_thread::yield();
            }
        }

    private:
        LaunchOptions m_launch_options { .mode = LaunchMode::Async, .explicitExecutor = nullptr };
        // Единый регистр: старший бит — смерть, младшие 63 бита — счетчик живых детей
        std::atomic<uint64_t> m_childCount{ 0 };
        std::coroutine_handle<> m_jointerTaskHandle;
        IExecutor* m_threadExecutor{};

        void incrementChildCount() noexcept;
        void decrementChildCount() noexcept;

        // Адрес публикации фрейма корутины-владельца этого скоупа
        std::atomic<void*> m_ownerHandle{ nullptr };

        CoroScope* m_parentScope{ nullptr };

        explicit CoroScope(LaunchOptions options, CoroScope* parentScope = {}) noexcept
            : m_launch_options(options), m_parentScope(parentScope) {
            m_parentScope->incrementChildCount();
        }

    };

} // namespace shuttle::engine

#endif //SHUTTLEENGINE_COROSCOPE_HPP
