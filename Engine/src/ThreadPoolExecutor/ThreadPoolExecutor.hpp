//
// Created by Shagu on 13.10.2026.
//

#ifndef SHUTTLEENGINE_THREADPOOLEXECUTOR_HPP
#define SHUTTLEENGINE_THREADPOOLEXECUTOR_HPP
#include "concurrentqueue.h"
#include "CoroutineWorkStealingQueue.hpp"
#include "Notifier.hpp"
#include <vector>
#include <bits/move_only_function.h>

#include "Coroutines/IExecuter.hpp"
#include "ResultCode.hpp"
#include "MiAllocator/MiAllocator.hpp"

namespace shuttle::engine {

    class ThreadPoolExecutor;

    struct WorkerContext {
        size_t m_id{ 0 };
        uint32_t m_randomState{ 0 };
        CoroutineWorkStealingQueue<> m_queue{};
        std::coroutine_handle<> m_dispatcher{};
        std::atomic<ResultCode> m_pendingInternalError{ ResultCode::SUCCESS };
        moodycamel::ConcurrentQueue<std::coroutine_handle<>> m_serviceQueue;
        std::atomic<uint64_t> m_serviceQueueSize{ 0 };

        // КЛАСС-РЕАЛИЗАЦИЯ СОБСТВЕННОГО ЭКЗЕКЬЮТОРА ВОРКЕРА
        class WorkerExecutor final : public IExecutor, public IThreadContinuationContext {
        public:
            WorkerExecutor() = default;
            explicit WorkerExecutor(WorkerContext* ctx, ThreadPoolExecutor* exec) noexcept : workerContext{ctx}, threadPoolExecutor{exec} {}

            ResultCode execute(std::coroutine_handle<> coro) noexcept override;

            // ПРИОРИТЕТНЫЙ ШАГ 0 ПЛАНИРОВЩИКА (С ПАКЕТНЫМ ЛИМИТОМ)
            std::coroutine_handle<> dequeueCoroutine() noexcept override;

            // БЕСПЛАТНЫЙ ВОЗВРАТ ИСПОЛНИТЕЛЯ
            IExecutor* getExecutor() noexcept override { return this; }
        private:
            WorkerContext* workerContext{ nullptr };
            ThreadPoolExecutor* threadPoolExecutor{ nullptr };
            void wakeUpWorker() const noexcept; // Вынесено в .cpp для развязки с ThreadPoolExecutor
        } m_continuation;
    };

    class ThreadPoolDispatcherTask {
    public:
        struct promise_type {
            ResultCode allocationStatus{ ResultCode::SUCCESS };

            // 1. Стандартный контракт C++20
            ThreadPoolDispatcherTask get_return_object() noexcept;
            static ThreadPoolDispatcherTask get_return_object_on_allocation_failure() noexcept;

            // 2. ВОТ ОНА — ВАША ИДЕЯ: Сохраняем хэндл на фазе initial_suspend!
            [[nodiscard]] auto initial_suspend() const noexcept {
                struct InitialDispatcherAwaiter {
                    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
                    [[nodiscard]] bool await_ready() const noexcept { return false; }

                    [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> myHandle) const noexcept {
                        // Атомарно сохраняем хэндл диспетчера в контекст текущего потока ОС Windows!
                        // Теперь воркер на 100% знает адрес своего парковочника.
                        worker->m_dispatcher = myHandle;
                        return myHandle;
                    }
                    void await_resume() const noexcept {}
                    WorkerContext* worker;
                };
                return InitialDispatcherAwaiter{worker};
            }

            // Диспетчер никогда не засыпает на выходе — он уничтожается явно в workerLoop
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }

            void return_void() const noexcept {}

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            void unhandled_exception() const noexcept { std::terminate(); } // -fno-exceptions

            // 3. Реактивный Symmetric Transfer прыжок
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            [[nodiscard]] auto yield_value(std::coroutine_handle<> nextHandle) const noexcept {
                struct awaiter {
                    std::coroutine_handle<> m_handle;

                    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
                    [[nodiscard]] bool await_ready() const noexcept { return false; }
                    [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                        return m_handle;
                    }
                    void await_resume() const noexcept {}
                };
                return awaiter{nextHandle};
            }

            promise_type(ThreadPoolExecutor&, WorkerContext& ctx) noexcept {
                worker = &ctx;
            }

            // Ваша кастомная lock-free аллокация на MiAllocator
            void* operator new(std::size_t size) noexcept { return MiAllocator<char>().allocate(size); }
            void operator delete(void* ptr) noexcept { MiAllocator<char>().deallocate(static_cast<char*>(ptr)); }

        private:
            WorkerContext* worker{nullptr};
        };

        explicit ThreadPoolDispatcherTask(std::coroutine_handle<promise_type> h) noexcept : handle(h), status(ResultCode::SUCCESS) {}
        explicit ThreadPoolDispatcherTask(ResultCode err) noexcept : handle(nullptr), status(err) {}

        std::coroutine_handle<promise_type> handle;
        ResultCode status;
    };

    class ThreadPoolExecutor final : public IExecutor {
        class ThreadWorkerContextGuard {
        public:
            ThreadWorkerContextGuard(WorkerContext* context) {
                m_currentWorker = context;
            }
            ~ThreadWorkerContextGuard() {
                m_currentWorker = nullptr;
            }
        };
    public:
        using WorkerCrashCallback = std::move_only_function<void (size_t id, ResultCode)>;

        [[nodiscard]] static std::expected<MiUniquePtr<ThreadPoolExecutor>, ResultCode> create(
            WorkerCrashCallback&& crushCallback = nullptr) noexcept {
            return create(std::thread::hardware_concurrency(), std::move(crushCallback));
        }

        [[nodiscard]] static std::expected<MiUniquePtr<ThreadPoolExecutor>, ResultCode> create(
            size_t numWorkers,
            WorkerCrashCallback&& crushCallback) noexcept;

        // Деструктор гарантирует мягкую и безопасную остановку пула
        ~ThreadPoolExecutor() override;

        // Перегрузка чисто виртуального метода интерфейса IExecutor
        ResultCode execute(std::coroutine_handle<> coro) override;

        ResultCode enqueueIncoming(std::coroutine_handle<> handle) noexcept;

        void signalWorkerWakeup(size_t size);

        explicit ThreadPoolExecutor() = default;// Конструктор инициализирует и сразу запускает потоки-воркеры

        explicit ThreadPoolExecutor(
            size_t numWorkers,
            std::vector<std::jthread, MiAllocator<std::jthread>> && threads,
            std::vector<WorkerContext, MiAllocator<WorkerContext>> && workers,
            moodycamel::ConcurrentQueue<std::coroutine_handle<>>&& incomingQueue,
            WorkerCrashCallback&& callback) noexcept :
        m_incomingQueue {std::move(incomingQueue)}, m_threads{std::move(threads)}, m_workers{std::move(workers)}, m_numWorkers{numWorkers}, m_crashCallback{std::move(callback)} {}

        // Извлечение задачи текущим потоком воркера (каскадный lock-free поиск)
        std::coroutine_handle<> dequeueCoroutine(WorkerContext *workerContext);

    private:

        ResultCode init() noexcept;

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
