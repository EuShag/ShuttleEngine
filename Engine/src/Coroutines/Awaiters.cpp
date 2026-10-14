//
// Created by Shagu on 12.10.2026.
//

#include "Awaiters.hpp"
#include "IExecuter.hpp"
#include "CoroScope.hpp"

namespace shuttle::engine {
    bool InitialAwaiter::await_ready() const noexcept {
        return coroScope->canImmediateLaunched();
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    std::coroutine_handle<> InitialAwaiter::await_suspend(std::coroutine_handle<>) const noexcept {
        return std::noop_coroutine();
    }

    std::coroutine_handle<> FinalAwaiter::await_suspend(std::coroutine_handle<> handle) const noexcept {
        if (m_scope) [[likely]] {
            if (auto nextHandle = m_scope->getTransferCoro(handle)) return nextHandle;
        }
        return IExecutor::getNextCoroutine();
    }
}


