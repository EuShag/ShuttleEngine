#include <async/one_thread_executor.hpp>
#include <async/try.hpp>

#include <array>
#include <atomic>
#include <coroutine>
#include <thread>

#include "async/coro_scope.hpp"

namespace {

    shuttle::async::Result<unsigned> increment(shuttle::async::Result<unsigned> input) {
        ASYNC_TRY(value, std::move(input));
        return value + 1;
    }

    struct TestCoroutine {
        struct promise_type {
            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                    return shuttle::async::IExecutor::getCurrentDispatcherCoroutine();
                }
                void await_resume() const noexcept {}
            };

            TestCoroutine get_return_object() noexcept {
                return TestCoroutine{
                    std::coroutine_handle<promise_type>::from_promise(*this)
                };
            }

            std::suspend_always initial_suspend() const noexcept { return {}; }
            FinalAwaiter final_suspend() const noexcept { return {}; }
            void return_void() const noexcept {}
            void unhandled_exception() const noexcept { std::terminate(); }
        };

        std::coroutine_handle<promise_type> handle;
    };

    TestCoroutine verifyExecutorContext(
        shuttle::async::OneThreadExecutor* executor,
        std::array<std::thread::id, 3>& workerIds,
        std::array<bool, 3>& validContexts,
        std::atomic<unsigned>& completed,
        unsigned index)
    {
        validContexts[index] =
            shuttle::async::IExecutor::getThreadPinnedExecutor() == executor
            && shuttle::async::IExecutor::getCurrentSharedExecutor() == executor
            && shuttle::async::IExecutor::getCurrentDispatcherCoroutine()
                != std::noop_coroutine();
        workerIds[index] = std::this_thread::get_id();
        completed.fetch_add(1, std::memory_order_release);
        completed.notify_one();
        co_return;
    }

    TestCoroutine countCompletion(std::atomic<unsigned>& completed) {
        completed.fetch_add(1, std::memory_order_release);
        co_return;
    }
}

int main() {
    auto incremented = increment(41u);
    if (!incremented || *incremented != 42
        || shuttle::async::to_string(shuttle::async::Error::executor_stopped) != "executor stopped") {
        return 9;
    }


    auto executorResult = shuttle::async::OneThreadExecutor::create();
    if (!executorResult) {
        return 1;
    }

    auto& executor = *executorResult;
    if (executor->execute({}).error() != shuttle::async::Error::invalid_coroutine_handle) {
        return 2;
    }

    std::array<std::thread::id, 3> workerIds{};
    std::array<bool, 3> validContexts{};
    std::atomic<unsigned> completed{0};

    auto first = verifyExecutorContext(
        executor.get(), workerIds, validContexts, completed, 0);
    auto second = verifyExecutorContext(
        executor.get(), workerIds, validContexts, completed, 1);
    auto completionFence = verifyExecutorContext(
        executor.get(), workerIds, validContexts, completed, 2);

    if (!executor->execute(first.handle)
        || !executor->execute(second.handle)
        || !executor->execute(completionFence.handle)) {
        return 3;
    }

    unsigned observed = completed.load(std::memory_order_acquire);
    while (observed != 3) {
        completed.wait(observed, std::memory_order_acquire);
        observed = completed.load(std::memory_order_acquire);
    }

    if (!validContexts[0] || !validContexts[1] || !validContexts[2]
        || workerIds[0] == std::this_thread::get_id()
        || workerIds[0] != workerIds[1]
        || workerIds[1] != workerIds[2]) {
        return 5;
    }

    first.handle.destroy();
    second.handle.destroy();
    executorResult.value().reset();
    completionFence.handle.destroy();

    constexpr unsigned TASK_COUNT = 512;
    constexpr unsigned PRODUCER_COUNT = 4;
    auto drainingExecutorResult = shuttle::async::OneThreadExecutor::create();
    if (!drainingExecutorResult) {
        return 6;
    }

    std::array<TestCoroutine, TASK_COUNT> tasks{};
    std::array<shuttle::async::Status, TASK_COUNT> enqueueResults{};
    std::atomic<unsigned> drainedTasks{0};
    for (auto& task : tasks) {
        task = countCompletion(drainedTasks);
    }

    std::array<std::thread, PRODUCER_COUNT> producers;
    for (unsigned producer = 0; producer < PRODUCER_COUNT; ++producer) {
        producers[producer] = std::thread([&, producer] {
            for (unsigned index = producer; index < TASK_COUNT; index += PRODUCER_COUNT) {
                enqueueResults[index] = drainingExecutorResult.value()->execute(tasks[index].handle);
            }
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }

    drainingExecutorResult.value().reset();

    if (drainedTasks.load(std::memory_order_acquire) != TASK_COUNT) {
        return 7;
    }
    for (unsigned index = 0; index < TASK_COUNT; ++index) {
        if (!enqueueResults[index]) {
            return 8;
        }
        tasks[index].handle.destroy();
    }
    return 0;
}
