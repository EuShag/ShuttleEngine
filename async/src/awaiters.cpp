//
// Created by Shagu on 12.10.2026.
//

#include <async/awaiters.hpp>
#include <async/coro_scope.hpp>

namespace shuttle::async {
    bool InitialAwaiter::await_ready() const noexcept {
        return coroScope->canImmediateLaunched();
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::coroutine_handle<> InitialAwaiter::await_suspend(std::coroutine_handle<>) const noexcept {
        return std::noop_coroutine();
    }

    std::coroutine_handle<> FinalAwaiter::await_suspend(std::coroutine_handle<> handle) const noexcept {
        if (m_scope) [[likely]] {
            return m_scope->getTransferCoro(handle);
        }
        return IExecutor::getCurrentDispatcherCoroutine();
    }
}