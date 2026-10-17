//
// Created by Shagu on 12.10.2026.
//
#include <async/coro_scope.hpp>
namespace shuttle::async {
    std::coroutine_handle<> JointerTask::Initial::await_suspend(std::coroutine_handle<> coroHandle) const noexcept {
        if (m_scopePtr) {
            // 1. СНАЧАЛА регистрируем хэндл джоинтера в скоупе, чтобы дети знали, кого будить!
            m_scopePtr->registerJointerTaskHandle(coroHandle, IExecutor::getThreadPinnedExecutor());
            // 2. ТОЛЬКО ПОСЛЕ ЭТОГО взводим атомарный барьер DEATH_SIGNAL
            if (auto handle = m_scopePtr->checkCanDestroyed(coroHandle); handle) return handle;
        }
        return IExecutor::getCurrentDispatcherCoroutine();
    }

    CoroScope CoroScope::createChild(LaunchOptions options) noexcept {
        return CoroScope{ options, this };
    }

    bool CoroScope::canImmediateLaunched() const noexcept { return m_launch_options.mode == LaunchMode::Immediate; }

    std::coroutine_handle<> CoroScope::getTransferCoro(std::coroutine_handle<> coroutineHandle) const noexcept {
        // 1. Извлекаем родителя ЗАРАНЕЕ, пока фрейм корутины гарантированно цел
        auto parent = m_parentScope->getCoroHandle();
        auto launchMode = m_launch_options.mode;
        coroutineHandle.destroy();
        // 3. Возвращаем родителя (или уходим в диспетчер, если это корень)
        if (parent && launchMode == LaunchMode::Immediate) return parent;
        return IExecutor::getCurrentDispatcherCoroutine();
    }

    std::coroutine_handle<> CoroScope::checkCanDestroyed(std::coroutine_handle<> coroHandle) noexcept {
        if (uint64_t const prevChildren = m_childCount.fetch_or(DEATH_SIGNAL, std::memory_order_acq_rel);
            (prevChildren & ~DEATH_SIGNAL) == 0) {
            // 2. Мы последние, атомарный барьер пройден. Уничтожаем фрейм кучи.
            return coroHandle;
        }
        return nullptr;
    }

    void CoroScope::incrementChildCount() noexcept {
        m_childCount.fetch_add(1, std::memory_order_relaxed);
    }

    void CoroScope::decrementChildCount() noexcept {
        if (uint64_t const prev = m_childCount.fetch_sub(1, std::memory_order_acq_rel); prev == DEATH_SIGNAL + 1) {
            auto const resumeResult = m_threadExecutor->execute(
                std::coroutine_handle<>::from_address(m_jointerTaskHandle.load(std::memory_order_acquire)));
            if (!resumeResult) [[unlikely]] {
                std::terminate();
            }
        }
    }

    void CoroScope::registerCoroHandle(std::coroutine_handle<> coroHandle) noexcept {
        m_ownerHandle.store(coroHandle.address(), std::memory_order_release);
    }

    std::coroutine_handle<> CoroScope::getCoroHandle() const noexcept {
        void* const addr = m_ownerHandle.load(std::memory_order_acquire);
        return std::coroutine_handle<>::from_address(addr);
    }

    void CoroScope::registerJointerTaskHandle(std::coroutine_handle<> coroHandle, IExecutor* ctx) noexcept {
        m_jointerTaskHandle.store(coroHandle.address(), std::memory_order_relaxed);
        m_threadExecutor = ctx;
    }
}
