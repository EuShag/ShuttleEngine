//
// Created by Shagu on 13.10.2026.
//

#include <async/thread_pool_executor/thread_pool_executor.hpp>
#include <async/try.hpp>

#include <xmmintrin.h>

namespace shuttle::async {
    Status WorkerContext::WorkerExecutor::execute(std::coroutine_handle<> coro) noexcept {
        if (!workerContext->m_serviceQueue.try_enqueue(coro)) {
            return std::unexpected(Error::queue_enqueue_failed);
        }
        workerContext->m_serviceQueueSize.fetch_add(1, std::memory_order_relaxed);
        wakeUpWorker();
        return {};
    }

    void WorkerContext::WorkerExecutor::wakeUpWorker() const noexcept {
        if (workerContext && threadPoolExecutor) [[likely]] {
            // Извлекаем уникальный системный ID воркера в пуле (например, индекс в массиве потоков)
            size_t const workerId = workerContext->m_id;

            // Передаем команду пулу потоков: "Проснись, для воркера №X прилетели служебные задачи!"
            // Метод пула атомарно взведет флаг и сделает WakeByAddressSingle / ReleaseSemaphore
            threadPoolExecutor->signalWorkerWakeup(workerId);
        }
    }

    // =================================================================
    // 2. ДИНАМИЧЕСКАЯ ФАБРИКА (Вся "грязная" работа по сборке и запуску)
    // =================================================================
    [[nodiscard]] Result<MiUniquePtr<ThreadPoolExecutor>> ThreadPoolExecutor::create(
        size_t numWorkers,
        WorkerCrashCallback&& crushCallback) noexcept
    {
        if (numWorkers == 0) {
            return std::unexpected(Error::invalid_worker_count);
        }

        // 1. Объявляем транзитные контейнеры на стеке фабрики
        std::optional<moodycamel::ConcurrentQueue<std::coroutine_handle<>>> localQueueOpt;
        std::optional<std::vector<WorkerContext, MiAllocator<WorkerContext>>> localContextsOpt;
        std::vector<std::jthread, MiAllocator<std::jthread>> localThreads;

        // 2. БРОНИРОВАННЫЙ БЛОК БЕЗОПАСНОСТИ
        try {
            localQueueOpt.emplace();
            localThreads.reserve(numWorkers);
            localContextsOpt.emplace(numWorkers);
        }
        catch (...) {
            return std::unexpected(Error::allocation_failure);
        }

        auto& localContexts = *localContextsOpt;
        auto& localQueue = *localQueueOpt;

        // Настраиваем ID и сбрасываем атомики ошибок в уже выделенной монолитной памяти
        for (size_t i = 0; i < numWorkers; ++i) {
            localContexts[i].m_id = i;
        }

        // 3. Собираем объект-оболочку экзекьютора.
        // Контейнеры безвозвратно улетают внутрь полей смарт-поинтера.
        auto executor = mi_make_unique_nothrow<ThreadPoolExecutor>(
            numWorkers,
            std::move(localThreads),
            std::move(localContexts),
            std::move(localQueue),
            std::move(crushCallback)
        );

        if (!executor) [[unlikely]] {
            return std::unexpected(Error::allocation_failure);
        }

        // 4. ОЖИВЛЕНИЕ ПУЛА (Вся грязная работа)
        // Теперь, когда executor полностью собран и выделился в куче через mi_make_unique_nothrow,
        // его адрес в памяти ЖЕЛЕЗОБЕТОННО СТАБИЛЕН. Указатель executor.get() больше никогда не изменится!
        // Мы можем безопасно вызывать init(), зная, что потоки Windows получат вечный, валидный адрес 'this'.
        auto initResult = executor->init();
        if (!initResult) [[unlikely]] {
            return std::unexpected(initResult.error());
        }

        // 5. УСПЕХ: Возвращаем живой, запущенный, пуленепробиваемый пул потоков!
        return std::move(executor);
    }

    ThreadPoolExecutor::~ThreadPoolExecutor() {
        m_stopping.store(true, std::memory_order_release);
        m_notifier.notify_all();
    }

    Status ThreadPoolExecutor::execute(
        std::coroutine_handle<> coro) {
        // 1. СТРОГИЙ БАРЬЕР НА ВХОДЕ
        if (!coro) [[unlikely]] {
            return std::unexpected(Error::invalid_coroutine_handle);
        }

        // Жесткая кирпичная стена при Shutdown пула: новые задачи больше не принимаются,
        // этот код "отпружинит" в Yield или launch, давая корутинам штатно закрыть RAII ресурсы.
        if (m_stopping.load(std::memory_order_acquire)) [[unlikely]] {
            return std::unexpected(Error::executor_stopped);
        }

        // 2. ОПРЕДЕЛЯЕМ КОНТЕКСТ ВЫЗОВА
        // Извлекаем TLS-указатель текущего физического потока Windows
        WorkerContext* currentCtx = m_currentWorker;

        if (currentCtx != nullptr) {
            // =================================================================
            // FAST-PATH (Вызов внутри пула): Корутина порождает корутину.
            // =================================================================
            // Толкаем задачу НАПРЯМУЮ в локальную деку Chase-Lev без единой блокировки
            if (!currentCtx->m_queue.try_push(coro)) [[unlikely]] {

                // ЛОКАЛЬНЫЙ КАРМАН ПЕРЕПОЛНЕН (>1024 задач):
                // Мы не падаем, а аварийно сгружаем излишек во входящую глобальную воронку.
                // Используем наш макрос VOID-группы, который вернет ошибку, если у Windows кончилась память!
                ASYNC_TRY_VOID(enqueueIncoming(coro));
                return {};
            }

            // Пинаем строго одного случайного заснувшего соседа через WaitOnAddress,
            // чтобы он проснулся и помог нам разгрузить бэклог (Work-Stealing)
            m_notifier.notify_one();
        }
        else {
            // =================================================================
            // SLOW-PATH (Внешний поток): Вызов из main(), сетевого IO или ОС-колбэка.
            // =================================================================
            // Спокойно и надежно сгружаем корневую задачу в глобальный lock-free буфер
            ASYNC_TRY_VOID(enqueueIncoming(coro));
        }

        return {};
    }

    // =================================================================
    // ВСПОМОГАТЕЛЬНЫЙ МЕТОД КЛАССА ДЛЯ РАБОТЫ С ГЛОБАЛЬНЫМ СКЛАДОМ
    // =================================================================
    Status ThreadPoolExecutor::enqueueIncoming(std::coroutine_handle<> handle) noexcept {
        // Атомарно инкрементируем глобальный счетчик бэклога
        m_incomingCount.fetch_add(1, std::memory_order_release);

        // moodycamel затребует новые страницы памяти у ОС Windows, если старые забиты.
        // Если в операционной системе тотально закончилась память — метод вернет false.
        if (!m_incomingQueue.enqueue(handle)) [[unlikely]] {
            // Откатываем счетчик назад
            m_incomingCount.fetch_sub(1, std::memory_order_relaxed);

            // Единственная законная техническая internalError воркера/пула:
            return std::unexpected(Error::queue_enqueue_failed);
        }

        // Будим одного воркера пула, чтобы он выгреб задачу из глобального склада
        m_notifier.notify_one();
        return {};
    }

    void ThreadPoolExecutor::signalWorkerWakeup(size_t) {
        m_notifier.notify_all();
    }

    Status ThreadPoolExecutor::init() noexcept {
        if (!m_threads.empty()) [[unlikely]] {
            return {};
        }

        for (size_t i = 0; i < m_numWorkers; ++i) {
            try {
                // Запускаем jthread. Передаем метод-петлю, адрес пула и индекс воркера
                m_threads.emplace_back(&ThreadPoolExecutor::workerLoop, this, i);
            }
            catch (const std::system_error&) {
                // 🚨 КРИТИЧЕСКИЙ СБОЙ ОС: Закончились лимиты дескрипторов Windows!
                m_stopping.store(true, std::memory_order_release);
                m_notifier.notify_all(); // Будим успевших уснуть

                // Деструкторы jthread сами сделают join() для всех успешно запущенных потоков.
                m_threads.clear();

                return std::unexpected(Error::worker_start_failed);
            }
        }

        return {};
    }

    void ThreadPoolExecutor::workerLoop(size_t id) noexcept {
        WorkerContext& ctx = m_workers[id];
        ctx.m_serviceQueueConsumerToken.emplace(ctx.m_serviceQueue);
        ctx.m_incomingQueueConsumerToken.emplace(m_incomingQueue);

        // Инициализируем вшитый контекст продолжения прямо перед пуском
        ctx.m_continuation = WorkerContext::WorkerExecutor{&ctx, this};
        ctx.m_randomState = static_cast<uint32_t>(id + 1) | 1u;

        auto dispatcher = runDispatcher(ctx);
        ctx.m_dispatcher = dispatcher;

        if (!dispatcher.has_value()) [[unlikely]] { m_crashCallback(ctx.m_id, dispatcher.error()); return; }
        ThreadContextGuard currentExecutorGuard({
            .m_sharedExecutor = this,
            .m_threadPinnedExecutor = &ctx.m_continuation,
            .m_dispatcherCoroHandle = dispatcher
        });

        ctx.m_dispatcher.resume();
    }

    ThreadPoolDispatcherTask ThreadPoolExecutor::runDispatcher(WorkerContext& ctx) {
        // =================================================================
        // ОСНОВНОЙ СИНХРОННЫЙ ЦИКЛ ПЛАНИРОВАНИЯ ТЕКУЩЕГО ПОТОКА
        // =================================================================
        uint32_t spin = 0;
        while (true) {
            // КРИТИЧЕСКИЙ ИНВАРИАНТ ОСТАНОВКИ ПУЛА (При выходе из игры)
            if (m_stopping.load(std::memory_order_relaxed)) [[unlikely]] {
                if (ctx.m_queue.empty()
                    && ctx.m_serviceQueueSize.load(std::memory_order_relaxed) == 0
                    && m_incomingCount.load(std::memory_order_relaxed) == 0) {
                    break; // Мягко тушим поток ОС Windows
                }
            }

            std::coroutine_handle targetHandle = nullptr;
            // ШАГ 0: Пакетная вычистка Шага 0 (Приоритетная служебная очередь)
            if (ctx.m_serviceQueue.try_dequeue(*ctx.m_serviceQueueConsumerToken, targetHandle) && targetHandle) {
                ctx.m_serviceQueueSize.fetch_sub(1, std::memory_order_acq_rel);
                spin = 0;
                co_yield targetHandle;
                continue;
            }

            // ШАГ 1: Локальная дека Chase-Lev (Молниеносный LIFO воркера)
            if (auto handle = ctx.m_queue.pop()) {
                spin = 0;
                co_yield handle;
                continue;
            }

            // ШАГ 2: Входящая глобальная воронка пула задач (FIFO общего склада)
            uint64_t const incomingCount = m_incomingCount.load(std::memory_order_acquire);
            if (incomingCount > 0) {
                std::coroutine_handle globalCoro = nullptr;
                if (m_incomingQueue.try_dequeue(*ctx.m_incomingQueueConsumerToken, globalCoro) && globalCoro) {
                    m_incomingCount.fetch_sub(1, std::memory_order_relaxed);
                    spin = 0;
                    co_yield globalCoro;
                    continue;
                }
            }

            // ШАГ 3: Рандомизированное Work-Stealing у коллег по пулу (Xorshift FIFO с головы)
            ctx.m_randomState ^= ctx.m_randomState << 13;
            ctx.m_randomState ^= ctx.m_randomState >> 17;
            ctx.m_randomState ^= ctx.m_randomState << 5;

            size_t const startVictimId = ctx.m_randomState % m_numWorkers;

            std::coroutine_handle stealCoroHandle = nullptr;
            for (size_t i = 0; i < m_numWorkers; ++i) {
                size_t const victimId = (startVictimId + i) % m_numWorkers;

                if (victimId != ctx.m_id) {
                    if (auto handle = m_workers[victimId].m_queue.steal()) {
                        stealCoroHandle = handle; // Задача успешно украдена у соседа!
                        break;
                    }
                }
            }
            if (stealCoroHandle != nullptr) {
                spin = 0;
                co_yield stealCoroHandle;
                continue;
            }

            // 1. АКТИВНЫЙ SPIN-WAIT (500 микро-кругов микро-паузы процессора)
            if (spin < 500) {
                #if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
                _mm_pause(); // Спасаем ядро CPU от теплового перегрева
                 #endif
                spin++;
            }
            else {
                // 2. ТЯЖЕЛЫЙ СТАТИЧЕСКИЙ СОН НА ВАШЕМ GENERATION NOTIFIER
                // Поток ОС Windows безболезненно засыпает в анабиозе ядра ОС через WaitOnAddress
                m_notifier.wait([this, &ctx]() noexcept {
                    if (m_stopping.load(std::memory_order_acquire)) [[unlikely]] return true;

                    // Проверяем точные лок-фри счетчики и деки без лишнего оверхеда
                    if (ctx.m_serviceQueueSize.load(std::memory_order_acquire) > 0) return true;
                    if (!ctx.m_queue.empty()) return true;
                    if (m_incomingCount.load(std::memory_order_acquire) > 0) return true;
                    for (size_t workerId = 0; workerId < m_numWorkers; ++workerId) {
                        if (workerId != ctx.m_id && !m_workers[workerId].m_queue.empty()) return true;
                    }

                    return false; // Всё пусто — продолжаем спать в ядре ОС
                });
                spin = 0;
            }
        }
        co_return;
    }

}
