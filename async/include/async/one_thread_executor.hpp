#pragma once

#include <async/thread_pool_executor/notifier.hpp>
#include <async/i_executor.hpp>
#include <async/mi_allocator.hpp>
#include <async/thread_pool_executor/thread_pool_dispatcher_task.hpp>

#include <concurrentqueue.h>

#include <atomic>
#include <coroutine>
#include <expected>
#include <cstdint>
#include <new>
#include <system_error>
#include <thread>

namespace shuttle::async {

    class OneThreadExecutor final : public IExecutor {
    public:
        OneThreadExecutor() noexcept = default;
        OneThreadExecutor(const OneThreadExecutor&) = delete;
        OneThreadExecutor& operator=(const OneThreadExecutor&) = delete;
        OneThreadExecutor(OneThreadExecutor&&) = delete;
        OneThreadExecutor& operator=(OneThreadExecutor&&) = delete;
        ~OneThreadExecutor() override;

        [[nodiscard]] static Result<MiUniquePtr<OneThreadExecutor>> create() noexcept;

        [[nodiscard]] Status start() noexcept;
        [[nodiscard]] Status execute(std::coroutine_handle<> coroutine) override;

    private:
        static constexpr uint64_t STOPPING_BIT = uint64_t{1} << 63;
        static constexpr uint64_t ACTIVE_SUBMISSIONS_MASK = ~STOPPING_BIT;
        static constexpr int START_NOT_STARTED = 0;
        static constexpr int START_IN_PROGRESS = 1;
        static constexpr int START_READY = 2;
        static constexpr int START_FAILED = 3;

        void workerLoop() noexcept;
        ThreadPoolDispatcherTask runDispatcher();

        moodycamel::ConcurrentQueue<std::coroutine_handle<>> m_queue;
        std::optional<moodycamel::ConsumerToken> m_queueConsumerToken;
        std::atomic<uint64_t> m_pendingCount{0};
        std::atomic<uint64_t> m_lifecycle{0};
        std::atomic<int> m_startState{START_NOT_STARTED};
        std::atomic<Error> m_startError{Error::worker_start_failed};
        Notifier m_notifier;
        std::thread m_worker;
    };

}
