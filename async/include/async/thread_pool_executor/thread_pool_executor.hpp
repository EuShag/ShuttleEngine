//
// Created by Shagu on 13.10.2026.
//

#ifndef SHUTTLEENGINE_THREADPOOLEXECUTOR_HPP
#define SHUTTLEENGINE_THREADPOOLEXECUTOR_HPP
#include "concurrentqueue.h"
#include <async/thread_pool_executor/coroutine_work_stealing_queue.hpp>
#include <async/thread_pool_executor/notifier.hpp>
#include <optional>
#include <vector>
#include <functional>

#include <async/i_executor.hpp>
#include <async/mi_allocator.hpp>
#include <async/thread_pool_executor/thread_pool_dispatcher_task.hpp>

namespace shuttle::async {

    class ThreadPoolExecutor;

    struct WorkerContext {
        size_t m_id{ 0 };
        uint32_t m_randomState{ 0 };
        CoroutineWorkStealingQueue<> m_queue{};
        std::coroutine_handle<> m_dispatcher{};
        moodycamel::ConcurrentQueue<std::coroutine_handle<>> m_serviceQueue;
        std::optional<moodycamel::ConsumerToken> m_serviceQueueConsumerToken;
        std::optional<moodycamel::ConsumerToken> m_incomingQueueConsumerToken;
        std::atomic<uint64_t> m_serviceQueueSize{ 0 };

        // КЛАСС-РЕАЛИЗАЦИЯ СОБСТВЕННОГО ЭКЗЕКЬЮТОРА ВОРКЕРА
        class WorkerExecutor final : public IExecutor {
        public:
            WorkerExecutor() = default;
            explicit WorkerExecutor(WorkerContext* ctx, ThreadPoolExecutor* exec) noexcept : workerContext{ctx}, threadPoolExecutor{exec} {}
            [[nodiscard]] Status execute(std::coroutine_handle<> coro) noexcept override;
        private:
            WorkerContext* workerContext{ nullptr };
            ThreadPoolExecutor* threadPoolExecutor{ nullptr };
            void wakeUpWorker() const noexcept; // Вынесено в .cpp для развязки с ThreadPoolExecutor
        } m_continuation;
    };

    class ThreadPoolExecutor final : public IExecutor {
    public:
        using WorkerCrashCallback = std::move_only_function<void (size_t id, Error)>;

        [[nodiscard]] static Result<MiUniquePtr<ThreadPoolExecutor>> create(
            WorkerCrashCallback&& crushCallback = nullptr) noexcept {
            return create(std::thread::hardware_concurrency(), std::move(crushCallback));
        }

        [[nodiscard]] static Result<MiUniquePtr<ThreadPoolExecutor>> create(
            size_t numWorkers,
            WorkerCrashCallback&& crushCallback) noexcept;

        // Деструктор гарантирует мягкую и безопасную остановку пула
        ~ThreadPoolExecutor() override;

        // Перегрузка чисто виртуального метода интерфейса IExecutor
        [[nodiscard]] Status execute(std::coroutine_handle<> coro) override;

        [[nodiscard]] Status enqueueIncoming(std::coroutine_handle<> handle) noexcept;

        void signalWorkerWakeup(size_t size);

        explicit ThreadPoolExecutor() = default;// Конструктор инициализирует и сразу запускает потоки-воркеры

        explicit ThreadPoolExecutor(
            size_t numWorkers,
            std::vector<std::jthread, MiAllocator<std::jthread>> && threads,
            std::vector<WorkerContext, MiAllocator<WorkerContext>> && workers,
            moodycamel::ConcurrentQueue<std::coroutine_handle<>>&& incomingQueue,
            WorkerCrashCallback&& callback) noexcept :
        m_incomingQueue {std::move(incomingQueue)}, m_threads{std::move(threads)}, m_workers{std::move(workers)}, m_numWorkers{numWorkers}, m_crashCallback{std::move(callback)} {}

    private:

        [[nodiscard]] Status init() noexcept;

        inline static thread_local WorkerContext* m_currentWorker{ nullptr };

        // Внутренняя функция инициализации и бесконечного цикла потока ОС
        void workerLoop(size_t id) noexcept;

        // Тело служебной корутины-диспетчера (содержит co_yield)
        ThreadPoolDispatcherTask runDispatcher(WorkerContext &ctx);

        // Глобальные входящие очереди
        moodycamel::ConcurrentQueue<std::coroutine_handle<>> m_incomingQueue;
        std::atomic<uint64_t> m_incomingCount{ 0 };

        // Нотификатор на std::atomic::wait (засыпание потоков под Windows)
        Notifier m_notifier{};

        // Контейнеры управления временем жизни потоков и контекстов
        std::vector<std::jthread, MiAllocator<std::jthread>> m_threads;
        std::vector<WorkerContext, MiAllocator<WorkerContext>> m_workers; // Вектор монолитных структур данных воркеров

        std::atomic<bool> m_stopping{ false };
        std::atomic<size_t> m_nextWorkerIndex{ 0 };
        size_t m_numWorkers{0};
        WorkerCrashCallback m_crashCallback{ nullptr };
    };
}

#endif //SHUTTLEENGINE_THREADPOOLEXECUTOR_HPP
