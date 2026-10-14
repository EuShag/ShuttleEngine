//
// Created by Shagu on 12.10.2026.
//

#ifndef SHUTTLEENGINE_IEXECUTER_HPP
#define SHUTTLEENGINE_IEXECUTER_HPP
#include <thread>

#include "ResultCode.hpp"

namespace shuttle::engine {

    class IExecutor;

    struct IThreadContinuationContext {
        virtual std::coroutine_handle<> dequeueCoroutine() noexcept = 0;
        virtual IExecutor* getExecutor() noexcept = 0;
        virtual ~IThreadContinuationContext() = default;
    };

    class IExecutor {
    public:
        virtual ~IExecutor() = default;
        virtual ResultCode execute(std::coroutine_handle<> coro) = 0;

        [[nodiscard]] static std::coroutine_handle<> getNextCoroutine() noexcept {
            if (currentThreadContinuationContext == nullptr) [[unlikely]] {
                return std::noop_coroutine();
            }
            return currentThreadContinuationContext->dequeueCoroutine();
        }
        [[nodiscard]] static IExecutor* getThreadBindExecutor() {
            if (currentThreadContinuationContext == nullptr) [[unlikely]] {
                return nullptr; // Страховка на случай вызова вне рантайма Shuttle Engine
            }
            return currentThreadContinuationContext->getExecutor();
        }
    protected:
        // RAII-страж контекста потока
        class [[nodiscard]] IThreadContinuationContextGuard {
        public:
            explicit IThreadContinuationContextGuard(IThreadContinuationContext* exec) noexcept {
                currentThreadContinuationContext = exec;
            }
            ~IThreadContinuationContextGuard() noexcept {
                currentThreadContinuationContext = nullptr;
            }
            IThreadContinuationContextGuard(const IThreadContinuationContextGuard&) = delete;
            IThreadContinuationContextGuard& operator=(const IThreadContinuationContextGuard&) = delete;
        };

    private:
        inline static thread_local IThreadContinuationContext* currentThreadContinuationContext = nullptr;
    };

}

#endif //SHUTTLEENGINE_IEXECUTER_HPP
