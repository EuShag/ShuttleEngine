//
// Created by Shagu on 12.10.2026.
//
#include "CoroScope.hpp"
namespace shuttle::engine {
    std::coroutine_handle<> JointerTask::Initial::await_suspend(std::coroutine_handle<> coroHandle) const noexcept {
        if (m_scopePtr) {
            // 1. СНАЧАЛА регистрируем хэндл джоинтера в скоупе, чтобы дети знали, кого будить!
            m_scopePtr->registerJointerTaskHandle(coroHandle, IExecutor::getThreadBindExecutor());
            // 2. ТОЛЬКО ПОСЛЕ ЭТОГО взводим атомарный барьер DEATH_SIGNAL
            return m_scopePtr->checkCanDestroyed(coroHandle);
        }
        return IExecutor::getNextCoroutine();
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
        return std::noop_coroutine();
    }

    std::coroutine_handle<> CoroScope::checkCanDestroyed(std::coroutine_handle<> coroHandle) noexcept {
        if (uint64_t const prevChildren = m_childCount.fetch_add(DEATH_SIGNAL, std::memory_order_acq_rel); prevChildren == 0) {
            // 2. Мы последние, атомарный барьер пройден. Уничтожаем фрейм кучи.
            return coroHandle;
        }
        return std::noop_coroutine();
    }

    void CoroScope::incrementChildCount() noexcept {
        m_childCount.fetch_add(1, std::memory_order_relaxed);
    }

    void CoroScope::decrementChildCount() noexcept {
        uint64_t const prev = m_childCount.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == (DEATH_SIGNAL + 1)) {
            void* const addr = m_ownerHandle.load(std::memory_order_acquire);
            if (addr) [[likely]] {
                m_threadExecutor->execute(m_jointerTaskHandle);
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
        m_jointerTaskHandle = coroHandle;
        m_threadExecutor = ctx;
    }
}
