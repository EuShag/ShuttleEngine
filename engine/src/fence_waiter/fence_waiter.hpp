#pragma once
#include "include_vulkan.hpp"
#include <concurrentqueue.h>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream> // Для логирования ошибок (опционально)

namespace shuttle::engine {

    struct FenceWaitTask {
        vk::Fence                         fence;
        std::function<void(vk::Result)>  callback;
    };

    class FenceWaiter {
    public:
        // Передаем кастомный хэндлер внутренних ошибок самого FenceWaiter
        using ErrorCallback = std::function<void(vk::Result, std::string const&)>;

        FenceWaiter() = default;

        explicit FenceWaiter(vk::Device device, vk::detail::DispatchLoaderDynamic const& dispatcher, ErrorCallback errorCallback = nullptr)
            : m_device(device), m_dispatcher{&dispatcher}, m_running(false), m_errorCallback(std::move(errorCallback))
        {
            if (m_device) start(m_device);
        }

        ~FenceWaiter() { stop(); }

        void start(vk::Device device) {
            if (m_running) return;
            m_device = device;
            m_running = true;
            m_workerThread = std::thread([this] {
                if (auto result = threadLoop(); result != vk::Result::eSuccess) {
                    internalErrorCode = result;
                    handleInternalError(result, std::string("Error message: ") + std::string(vk::to_string(result)));
                }
            });
        }

        void stop() {
            m_running = false;
            if (m_workerThread.joinable()) m_workerThread.join();

            // Очищаем пулы при остановке
            vk::UniqueFence discarded;
            while (m_freeFences.try_dequeue(discarded)) {}
        }

        // ВЫДЕЛЕНИЕ СВОБОДНОГО ФЕНСА ДЛЯ ИСПОЛЬЗОВАНИЯ (Вызывается из CPU/Рендер-потока)
        [[nodiscard]] vk::ResultValue<vk::Fence> acquireFence() {

            if (internalErrorCode != vk::Result::eSuccess) {
                return {internalErrorCode, {}};
            }

            vk::UniqueFence uniqueFence;

            // Если в lock-free пуле есть готовый фенс — забираем хэндл
            if (m_freeFences.try_dequeue(uniqueFence)) {
                vk::Fence fenceHandle = uniqueFence.get();
                // Передаем владение во внутреннюю карту или просто сохраняем хэндл.
                // Чтобы не терять Unique-объект, мы можем временно переместить его во внутренний трекер,
                // либо, если пулинг чистый, хранить хэндлы. Для простоты пулинга UniqueFence:
                m_activeFencesTracker.enqueue(std::move(uniqueFence));
                return {vk::Result::eSuccess, fenceHandle};
            }

            auto [result, newUnique] = m_device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}, nullptr, *m_dispatcher);
            if (result != vk::Result::eSuccess) { return {result, {}}; }
            vk::Fence fenceHandle = *newUnique;

            m_activeFencesTracker.enqueue(std::move(newUnique));
            return {vk::Result::eSuccess, fenceHandle};
        }

        vk::Result addWaitTask(vk::Fence fence, std::function<void(vk::Result)> callback) {
            if (internalErrorCode != vk::Result::eSuccess) {
                return internalErrorCode;
            }
            m_taskQueue.enqueue({.fence = fence, .callback = std::move(callback)});
            return vk::Result::eSuccess;
        }

    private:
        // Хелпер для безопасного сброса и возврата фенса в пул
        vk::Result recycleFence(vk::Fence fence) {
            // Нативный сброс фенса в Vulkan API
            if (auto resetRes = m_device.resetFences(1, &fence, *m_dispatcher); resetRes != vk::Result::eSuccess) {
                return resetRes;
            }

            // Ищем соответствующий UniqueFence во временном трекере и возвращаем в пул свободных
            vk::UniqueFence uniqueFence;
            if (m_activeFencesTracker.try_dequeue(uniqueFence)) {
                // Проверяем, тот ли это фенс (в идеале очереди синхронизированы по порядку FIFO)
                if (uniqueFence.get() == fence) {
                    m_freeFences.enqueue(std::move(uniqueFence));
                } else {
                    // Если порядок нарушен (редко для кадров), возвращаем обратно в трекер
                    m_activeFencesTracker.enqueue(std::move(uniqueFence));
                }
            }
            return vk::Result::eSuccess;
        }

        // Централизованная обработка критических сбоев Vulkan (Device Lost и т.д.)
        void handleInternalError(vk::Result res, std::string const& message) const {
            if (m_errorCallback) {
                m_errorCallback(res, "[FenceWaiter Error] " + message);
            } else {
                std::cerr << "[FenceWaiter Critical] " << message << " | Code: " << vk::to_string(res) << std::endl;
            }
        }

        vk::Result threadLoop() {
            std::vector<vk::Fence>                         fenceBuffer;
            std::vector<std::function<void(vk::Result)>>   callbackBuffer;

            std::vector<vk::Fence>                         nextFenceBuffer;
            std::vector<std::function<void(vk::Result)>>   nextCallbackBuffer;

            FenceWaitTask newTask;

            while (m_running.load(std::memory_order_relaxed) || !fenceBuffer.empty()) {

                // ФАЗА 1: Опрос текущих активных фенсов в Vulkan
                if (!fenceBuffer.empty()) {
                    auto waitRes = m_device.waitForFences(
                        static_cast<uint32_t>(fenceBuffer.size()), fenceBuffer.data(),
                        vk::False, 2'000'000, *m_dispatcher
                    );

                    if (waitRes != vk::Result::eTimeout) {
                        if (waitRes != vk::Result::eSuccess) {
                            return waitRes;
                        }

                        for (size_t i = 0; i < fenceBuffer.size(); ++i) {
                            vk::Result fenceRes = vk::Result::eNotReady;
                            fenceRes = m_device.getFenceStatus(fenceBuffer[i], *m_dispatcher);

                            if (fenceRes != vk::Result::eNotReady) {
                                // ФЕНС ОТРАБОТАЛ (Success или Критический сбой)
                                if (callbackBuffer[i]) {
                                    callbackBuffer[i](fenceRes);
                                }

                                if (fenceRes == vk::Result::eSuccess) {
                                    // Если всё ок — сбрасываем и возвращаем в пул свободных
                                    auto recycleResult = recycleFence(fenceBuffer[i]);
                                    if (recycleResult != vk::Result::eSuccess) {
                                        return recycleResult;
                                    }
                                } else {
                                    // ПАТЧ №1: ФЕНС ВЕРНУЛ ОШИБКУ (Device Lost)
                                    // Мы аккуратно извлекаем ТОЛЬКО ЭТОТ конкретный фенс из активного трекера,
                                    // не трогая чужие живые фенсы!

                                    // Вспомогательный буфер для временной переливки
                                    moodycamel::ConcurrentQueue<vk::UniqueFence> tempTracker;
                                    vk::UniqueFence currentActive;

                                    // Переливаем элементы, пока не найдем наш поврежденный фенс
                                    while (m_activeFencesTracker.try_dequeue(currentActive)) {
                                        if (currentActive.get() == fenceBuffer[i]) {
                                            vk::UniqueFence deadFence = std::move(currentActive); // Нашли! Забираем его на уничтожение (RAII)
                                            break;
                                        }
                                        tempTracker.enqueue(std::move(currentActive));
                                    }

                                    // Возвращаем остальные живые фенсы обратно на базу
                                    while (tempTracker.try_dequeue(currentActive)) {
                                        m_activeFencesTracker.enqueue(std::move(currentActive));
                                    }

                                    // ПАТЧ №2: УДАЛЕН ДУБЛИРУЮЩИЙ ВЫЗОВ КОЛБЭКА!
                                    // Он уже выполнился строчкой выше: `if (callbackBuffer[i]) callbackBuffer[i](fenceRes);`
                                    // Даем deadFence спокойно умереть по выходу из блока `{}` (RAII-деструкция)
                                }
                            } else {
                                // Фенс еще выполняется на GPU
                                nextFenceBuffer.push_back(fenceBuffer[i]);
                                nextCallbackBuffer.push_back(std::move(callbackBuffer[i]));
                            }
                        }
                    }
                }

                // ФАЗА 2: Забираем новичков из lock-free очереди
                while (m_taskQueue.try_dequeue(newTask)) {
                    vk::Result instantRes = vk::Result::eNotReady;
                    instantRes = m_device.getFenceStatus(newTask.fence, *m_dispatcher);

                    if (instantRes != vk::Result::eNotReady) {
                        if (newTask.callback) {
                            newTask.callback(instantRes);
                        }
                        if (instantRes == vk::Result::eSuccess) {
                            recycleFence(newTask.fence);
                        } else {
                            // Точечно выковыриваем мертвый фенс новичка из трекера
                            moodycamel::ConcurrentQueue<vk::UniqueFence> tempTracker;
                            vk::UniqueFence currentActive;

                            while (m_activeFencesTracker.try_dequeue(currentActive)) {
                                if (currentActive.get() == newTask.fence) {
                                    vk::UniqueFence deadFence = std::move(currentActive);
                                    break;
                                }
                                tempTracker.enqueue(std::move(currentActive));
                            }
                            while (tempTracker.try_dequeue(currentActive)) {
                                m_activeFencesTracker.enqueue(std::move(currentActive));
                            }

                            // ДУБЛИРУЮЩИЙ ВЫЗОВ КОЛБЭКА УДАЛЕН! Он уже отработал выше в коде.
                        }
                        continue;
                    }

                    nextFenceBuffer.push_back(newTask.fence);
                    nextCallbackBuffer.push_back(std::move(newTask.callback));
                }

                fenceBuffer = std::move(nextFenceBuffer);
                callbackBuffer = std::move(nextCallbackBuffer);

                nextFenceBuffer.clear();
                nextCallbackBuffer.clear();

                if (fenceBuffer.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            return vk::Result::eSuccess;
        }

        vk::Device m_device;
        vk::detail::DispatchLoaderDynamic const* m_dispatcher{};
        std::atomic<bool> m_running;
        std::thread m_workerThread;

        vk::Result internalErrorCode {vk::Result::eSuccess};

        moodycamel::ConcurrentQueue<FenceWaitTask> m_taskQueue;
        moodycamel::ConcurrentQueue<vk::UniqueFence> m_freeFences;

        // Трекер активных уникальных фенсов, чтобы не терять владение UniqueFence в рантайме
        moodycamel::ConcurrentQueue<vk::UniqueFence> m_activeFencesTracker;

        ErrorCallback m_errorCallback;
    };

} // namespace shuttle::engine
