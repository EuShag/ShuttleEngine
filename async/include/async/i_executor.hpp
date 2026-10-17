//
// Created by Shagu on 12.10.2026.
//

#ifndef SHUTTLEENGINE_IEXECUTER_HPP
#define SHUTTLEENGINE_IEXECUTER_HPP

#include <coroutine>

#include <async/error.hpp>

namespace shuttle::async {

    class IExecutor;

    struct ThreadContext {
        IExecutor* m_sharedExecutor{};
        IExecutor* m_threadPinnedExecutor{};
        std::coroutine_handle<> m_dispatcherCoroHandle{};
    };

    class IExecutor {
    public:
        virtual ~IExecutor() = default;
        [[nodiscard]] virtual Status execute(std::coroutine_handle<> coro) = 0;

        // =================================================================
        // 1. ПРИВЯЗАННЫЙ ИСПОЛНИТЕЛЕЙ (Thread-Pinned / Confined)
        // =================================================================
        // Возвращает экзекьютор, который намертво привязан к ТЕКУЩЕМУ воркеру.
        // Любой co_await на этом экзекьюторе вернет корутину строго на это же ядро CPU
        // через наш приоритетный Шаг 0 и Hot-Seat слот!
        [[nodiscard]] static IExecutor* getThreadPinnedExecutor() noexcept {
            return currentThreadContext.m_threadPinnedExecutor;
        }

        // =================================================================
        // 2. ОБЩИЙ ПУЛОВОЙ ИСПОЛНИТЕЛЬ (SharedPool / Unconfined / Balanced)
        // =================================================================
        // Возвращает тяжелый многопоточный исполнитель, которому принадлежит этот поток.
        // Задача, отправленная сюда, упадет на общий склад пула, и ее сможет
        // свободно забрать или украсть (Work-Stealing) любой другой фоновый воркер!
        [[nodiscard]] static IExecutor* getCurrentSharedExecutor() noexcept {
            return currentThreadContext.m_sharedExecutor;
        }

        [[nodiscard]] static std::coroutine_handle<> getCurrentDispatcherCoroutine() noexcept {
            return currentThreadContext.m_dispatcherCoroHandle;
        }
    protected:
        // RAII-страж контекста потока
        class [[nodiscard]] ThreadContextGuard {
        public:
            explicit ThreadContextGuard(ThreadContext context) noexcept
                : m_previousContext{currentThreadContext} {
                currentThreadContext = context;
            }
            ~ThreadContextGuard() noexcept {
                currentThreadContext = m_previousContext;
            }
            ThreadContextGuard(const ThreadContextGuard&) = delete;
            ThreadContextGuard& operator=(const ThreadContextGuard&) = delete;

        private:
            ThreadContext m_previousContext;
        };

    private:
        inline static thread_local ThreadContext currentThreadContext = {};
    };

}

#endif //SHUTTLEENGINE_IEXECUTER_HPP
