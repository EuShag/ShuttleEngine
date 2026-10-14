//
// Created by Shagu on 13.10.2026.
//

#include "ThreadPoolExecutor.hpp"

#include <xmmintrin.h>

namespace shuttle::engine {

    std::coroutine_handle<> WorkerContext::WorkerExecutor::dequeueCoroutine() noexcept {
        // Иначе идем за обычными задачами пула
        return threadPoolExecutor->dequeueCoroutine(workerContext);
    }

    ResultCode WorkerContext::WorkerExecutor::execute(std::coroutine_handle<> jointerCoro) noexcept {
        if (!jointerCoro) [[unlikely]] return ResultCode::EXECUTOR_INVALID_COROUTINE_HANDLE;

        if (!workerContext->m_serviceQueue.enqueue(jointerCoro)) [[unlikely]] {
            return ResultCode::ALLOCATION_FAILURE;
        }

        // ВАШЕ СПАСЕНИЕ: Атомарно взводим флаг Шага 0 в ОЗУ!
        // Теперь предикат сна физически не сможет вернуть false!
        workerContext->m_serviceQueueSize.fetch_add(1, std::memory_order_relaxed);

        // Только теперь пинаем системный notifier
        this->wakeUpWorker();
        return ResultCode::SUCCESS;
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

    ThreadPoolDispatcherTask ThreadPoolDispatcherTask::promise_type::get_return_object() noexcept {
        return ThreadPoolDispatcherTask{ std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    ThreadPoolDispatcherTask ThreadPoolDispatcherTask::promise_type::
    get_return_object_on_allocation_failure() noexcept {
        return ThreadPoolDispatcherTask{ ResultCode::ALLOCATION_FAILURE };
    }

    // =================================================================
    // 2. ДИНАМИЧЕСКАЯ ФАБРИКА (Вся "грязная" работа по сборке и запуску)
    // =================================================================
    [[nodiscard]] std::expected<MiUniquePtr<ThreadPoolExecutor>, ResultCode> ThreadPoolExecutor::create(
        size_t numWorkers,
        WorkerCrashCallback&& crushCallback) noexcept
    {
        if (numWorkers == 0) {
            return ::std::unexpected(ResultCode::EXECUTOR_INVALID_WORKER_COUNT);
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
            return ::std::unexpected(ResultCode::ALLOCATION_FAILURE);
        }

        auto& localContexts = *localContextsOpt;
        auto& localQueue = *localQueueOpt;

        // Настраиваем ID и сбрасываем атомики ошибок в уже выделенной монолитной памяти
        for (size_t i = 0; i < numWorkers; ++i) {
            localContexts[i].m_id = i;
            localContexts[i].m_pendingInternalError.store(ResultCode::SUCCESS, std::memory_order_relaxed);
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
            return ::std::unexpected(ResultCode::ALLOCATION_FAILURE);
        }

        // 4. ОЖИВЛЕНИЕ ПУЛА (Вся грязная работа)
        // Теперь, когда executor полностью собран и выделился в куче через mi_make_unique_nothrow,
        // его адрес в памяти ЖЕЛЕЗОБЕТОННО СТАБИЛЕН. Указатель executor.get() больше никогда не изменится!
        // Мы можем безопасно вызывать init(), зная, что потоки Windows получат вечный, валидный адрес 'this'.
        ResultCode initResult = executor->init();
        if (initResult != ResultCode::SUCCESS) [[unlikely]] {
            return ::std::unexpected(initResult);
        }

        // 5. УСПЕХ: Возвращаем живой, запущенный, пуленепробиваемый пул потоков!
        return std::move(executor);
    }

    ThreadPoolExecutor::~ThreadPoolExecutor() {
        m_stopping.store(true, std::memory_order_release);
        m_notifier.notify_all();
    }

    ResultCode ThreadPoolExecutor::execute(
        std::coroutine_handle<> coro) {
        // 1. СТРОГИЙ БАРЬЕР НА ВХОДЕ
        if (!coro) [[unlikely]] {
            return ResultCode::EXECUTOR_INVALID_COROUTINE_HANDLE;
        }

        // Жесткая кирпичная стена при Shutdown пула: новые задачи больше не принимаются,
        // этот код "отпружинит" в Yield или launch, давая корутинам штатно закрыть RAII ресурсы.
        if (m_stopping.load(std::memory_order_acquire)) [[unlikely]] {
            return ResultCode::EXECUTOR_STOPPED;
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
                SHUTTLE_TRY_VOID(enqueueIncoming(coro));
                return ResultCode::SUCCESS;
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
            SHUTTLE_TRY_VOID(enqueueIncoming(coro));
        }

        return ResultCode::SUCCESS;
    }

    // =================================================================
    // ВСПОМОГАТЕЛЬНЫЙ МЕТОД КЛАССА ДЛЯ РАБОТЫ С ГЛОБАЛЬНЫМ СКЛАДОМ
    // =================================================================
    ResultCode ThreadPoolExecutor::enqueueIncoming(std::coroutine_handle<> handle) noexcept {
        // Атомарно инкрементируем глобальный счетчик бэклога
        m_incomingCount.fetch_add(1, std::memory_order_release);

        // moodycamel затребует новые страницы памяти у ОС Windows, если старые забиты.
        // Если в операционной системе тотально закончилась память — метод вернет false.
        if (!m_incomingQueue.enqueue(handle)) [[unlikely]] {
            // Откатываем счетчик назад
            m_incomingCount.fetch_sub(1, std::memory_order_relaxed);

            // Единственная законная техническая internalError воркера/пула:
            return ResultCode::ALLOCATION_FAILURE;
        }

        // Будим одного воркера пула, чтобы он выгреб задачу из глобального склада
        m_notifier.notify_one();
        return ResultCode::SUCCESS;
    }

    void ThreadPoolExecutor::signalWorkerWakeup(size_t) {
        m_notifier.notify_all();
    }

    ResultCode ThreadPoolExecutor::init() noexcept {
        if (!m_threads.empty()) [[unlikely]] {
            return ResultCode::SUCCESS;
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

                return ResultCode::EXECUTOR_WORKER_START_FAILED;
            }
        }

        return ResultCode::SUCCESS;
    }

    std::coroutine_handle<> ThreadPoolExecutor::dequeueCoroutine(WorkerContext* workerContext) {
        if (!workerContext) [[unlikely]] return std::noop_coroutine();

        std::coroutine_handle targetHandle = nullptr;

        // Пакетная вычистка Шага 0
        if (workerContext->m_serviceQueue.try_dequeue(targetHandle) && targetHandle) {
            // Если очередь опустела, атомарно гасим флаг
            if (workerContext->m_serviceQueue.size_approx() == 0) {
                workerContext->m_serviceQueueSize.fetch_sub(1, std::memory_order_relaxed);
            }
            return targetHandle;
        }

        // ШАГ 1: Локальная дека Chase-Lev (Молниеносный LIFO)
        if (auto handle = workerContext->m_queue.pop()) return handle;

        // ШАГ 2: Входящая глобальная воронка moodycamel (FIFO)
        uint64_t incomingCount = m_incomingCount.load(std::memory_order_acquire);
        if (incomingCount > 0) {
            std::coroutine_handle globalCoro = nullptr;
            if (m_incomingQueue.try_dequeue(globalCoro) && globalCoro) {
                m_incomingCount.fetch_sub(1, std::memory_order_relaxed);
                return globalCoro;
            }
        }

        // ШАГ 3: Рандомизированное Work-Stealing у коллег (Xorshift FIFO с головы)
        workerContext->m_randomState ^= workerContext->m_randomState << 13;
        workerContext->m_randomState ^= workerContext->m_randomState >> 17;
        workerContext->m_randomState ^= workerContext->m_randomState << 5;

        size_t const startVictimId = workerContext->m_randomState % m_numWorkers;

        // 2. Бежим по кругу (циклу), опрашивая ВСЕХ соседей, начиная со случайной позиции
        for (size_t i = 0; i < m_numWorkers; ++i) {
            // Вычисляем циклический индекс жертвы
            size_t const victimId = (startVictimId + i) % m_numWorkers;

            // Защита от кражи у самого себя: свою очередь мы уже проверили на Шаге 1
            if (victimId != workerContext->m_id) {
                // Пытаемся украсть. Если натолкнулись на Stealing Race (гонку) —
                // мы не сдаемся, а идем проверять следующего соседа!
                if (auto handle = m_workers[victimId].m_queue.steal()) {
                    return handle; // УРА! Задача поймана с гарантией!
                }
            }
        }

        // Задач нет — мгновенно возвращаем nullptr диспетчеру для парковки
        return workerContext->m_dispatcher;
    }

    void ThreadPoolExecutor::workerLoop(size_t id) noexcept {
        WorkerContext& ctx = m_workers[id];
        // Инициализируем вшитый контекст продолжения прямо перед пуском
        ctx.m_continuation = WorkerContext::WorkerExecutor{&ctx, this};
        ctx.m_randomState = static_cast<uint32_t>(id + 1) | 1u;

        IThreadContinuationContextGuard currentExecutorGuard(&ctx.m_continuation);
        auto dispatcher = runDispatcher(ctx);

        if (is_error(dispatcher.status)) [[unlikely]] { m_crashCallback(ctx.m_id, dispatcher.status); }

        ctx.m_dispatcher = {};
    }

    ThreadPoolDispatcherTask ThreadPoolExecutor::runDispatcher(WorkerContext& ctx) {
        while (true) {
            std::coroutine_handle<> next = this->dequeueCoroutine(&ctx);

            // Если задач нет И пул останавливается — гасим свет и выходим
            // (Метод dequeCoroutine вернул ctx._dispatcher, но мы проверяем реальную пустоту очередей)
            if (ctx.m_queue.empty() && m_incomingCount.load(std::memory_order_relaxed) == 0 && m_stopping.load(std::memory_order_relaxed)) {
                break;
            }

            // ВАШ ТРЮК В ДЕЙСТВИИ:
            // Если dequeCoroutine() вернул хэндл нашего собственного диспетчера,
            // это маркер: задач нет нигде в движке вообще! Мы обязаны уснуть на нотификаторе.
            // 3. ЗАДАЧ НЕТ (ИЛИ ВЕРНУЛСЯ ДИСПЕТЧЕР) — ВКЛЮЧАЕМ АКТИВНЫЙ SPIN-WAIT!
            if (next == ctx.m_dispatcher && !m_stopping.load(std::memory_order_relaxed)) {

                std::coroutine_handle spinTask = nullptr;

                // Делаем 500 быстрых итераций активного микро-ожидания
                for (uint32_t spin = 0; spin < 500; ++spin) {
                    #if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
                    _mm_pause(); // Защита конвейера CPU от перегрева
                    #endif

                    // ВАША ИДЕЯ: Вместо слепой проверки флагов — делаем полноценную попытку взять задачу!
                    // Это проверит и локальный карман, и глобальную ConcurrentQueue, и украдет у соседа по Xorshift!
                    spinTask = this->dequeueCoroutine(&ctx);

                    // Если задача успешно выужена или украдена — немедленно вылетаем из спин-лока!
                    if (spinTask != ctx.m_dispatcher) {
                        break;
                    }
                }

                // Если спин-лок увенчался успехом и принес реальную корутину —
                // мы МГНОВЕННО прыгаем в неё по Symmetric Transfer, минуя уход в тяжелое ядро ОС!
                if (spinTask != ctx.m_dispatcher && spinTask != nullptr) {
                    co_yield spinTask;
                    continue; // Возвращаемся на следующий круг, когда задача завершится
                }

                // 4. ТЯЖЕЛЫЙ СТАТИЧЕСКИЙ СОН (Если за 500 кругов активного поиска ничего не нашли)
                m_notifier.wait([this, &ctx] () noexcept {
                    // Инвариант 0: Сигнал остановки пула (Принудительный подъем для выхода)
                    if (m_stopping.load(std::memory_order_acquire)) [[unlikely]] return true;
                        // ВАШ ИНВАРИАНТ: ПРИОРИТЕТНЫЙ ШАГ 0 (Проверяем личную служебную очередь!)
                        // Если прилетел JointerTask, AsyncMutex или AsyncCV — просыпаемся немедленно!
                        if (ctx.m_serviceQueueSize > 0) return true;

                        // Инвариант 1: Локальная дека Chase-Lev текущего воркера
                        if (!ctx.m_queue.empty()) return true;

                        // Инвариант 2: Входящая глобальная воронка пула (Общий склад задач)
                        if (m_incomingCount.load(std::memory_order_acquire) > 0) return true;

                        // И ВСЁ! Никаких циклов for по чужим декам!
                        // Если нагрянет глобальный голод, воркеры будут спать детерминировано.
                        // А если соседний воркер получит 100 задач, он сам вызовет notify_one(),
                        // чтобы поднять одного коллегу на Work-Stealing, когда дойдет до дела!
                        return false;
                    }
                );
                continue;
            }

            // Если вернулась реальная полезная корутина — прыгаем в неё!
            co_yield next;
        }
        co_return;
    }
}
