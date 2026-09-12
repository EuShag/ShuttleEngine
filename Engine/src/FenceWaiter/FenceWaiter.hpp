#pragma once
#include <vulkan/vulkan.hpp>
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

        explicit FenceWaiter(vk::Device const device = vk::Device{}, ErrorCallback errorCallback = nullptr)
            : m_device(device), m_running(false), m_errorCallback(std::move(errorCallback))
        {
            if (m_device) start(m_device);
        }

        ~FenceWaiter() { stop(); }

        void start(vk::Device device) {
            if (m_running) return;
            m_device = device;
            m_running = true;
            m_workerThread = std::thread(&FenceWaiter::threadLoop, this);
        }

        void stop() {
            m_running = false;
            if (m_workerThread.joinable()) m_workerThread.join();

            // Очищаем пулы при остановке
            vk::UniqueFence discarded;
            while (m_freeFences.try_dequeue(discarded)) {}
        }

        // ВЫДЕЛЕНИЕ СВОБОДНОГО ФЕНСА ДЛЯ ИСПОЛЬЗОВАНИЯ (Вызывается из CPU/Рендер-потока)
        [[nodiscard]] vk::Fence acquireFence() {
            vk::UniqueFence uniqueFence;

            // Если в lock-free пуле есть готовый фенс — забираем хэндл
            if (m_freeFences.try_dequeue(uniqueFence)) {
                vk::Fence fenceHandle = uniqueFence.get();
                // Передаем владение во внутреннюю карту или просто сохраняем хэндл.
                // Чтобы не терять Unique-объект, мы можем временно переместить его во внутренний трекер,
                // либо, если пулинг чистый, хранить хэндлы. Для простоты пулинга UniqueFence:
                m_activeFencesTracker.enqueue(std::move(uniqueFence));
                return fenceHandle;
            }

            // Если пул пуст — создаем новый фенс (изначально сброшенный / signaled)
            vk::FenceCreateInfo createInfo(vk::FenceCreateFlagBits::eSignaled);
            auto newUnique = m_device.createFenceUnique(createInfo);
            vk::Fence fenceHandle = newUnique.get();

            m_activeFencesTracker.enqueue(std::move(newUnique));
            return fenceHandle;
        }

        void addWaitTask(vk::Fence fence, std::function<void(vk::Result)> callback) {
            m_taskQueue.enqueue({fence, std::move(callback)});
        }

    private:
        // Хелпер для безопасного сброса и возврата фенса в пул
        void recycleFence(vk::Fence fence) {
            // Нативный сброс фенса в Vulkan API
            if (auto resetRes = m_device.resetFences(1, &fence); resetRes != vk::Result::eSuccess) {
                handleInternalError(resetRes, "Reset fence failed");
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
        }

        // Централизованная обработка критических сбоев Vulkan (Device Lost и т.д.)
        void handleInternalError(vk::Result res, std::string const& message) const {
            if (m_errorCallback) {
                m_errorCallback(res, "[FenceWaiter Error] " + message);
            } else {
                std::cerr << "[FenceWaiter Critical] " << message << " | Code: " << vk::to_string(res) << std::endl;
            }
        }

        void threadLoop() {
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
                        VK_FALSE, 2'000'000
                    );
                    if (waitRes != vk::Result::eSuccess) {
                        handleInternalError(waitRes, "Wait fences internal error");
                    }

                    for (size_t i = 0; i < fenceBuffer.size(); ++i) {
                        vk::Result fenceRes = vk::Result::eNotReady;
                        fenceRes = m_device.getFenceStatus(fenceBuffer[i]);

                        if (fenceRes != vk::Result::eNotReady) {
                            // ФЕНС ОТРАБОТАЛ (Success или Критический сбой)
                            if (callbackBuffer[i]) {
                                callbackBuffer[i](fenceRes);
                            }

                            if (fenceRes == vk::Result::eSuccess) {
                                // Если всё ок — сбрасываем и возвращаем в пул свободных
                                recycleFence(fenceBuffer[i]);
                            } else {
                                // Если фенс вернул ошибку (Device Lost) — МЫ ЕГО УНИЧТОЖАЕМ (выкидываем из трекера)
                                vk::UniqueFence deadFence;
                                m_activeFencesTracker.try_dequeue(deadFence); // Просто даем ему умереть (RAII)
                                handleInternalError(fenceRes, "Fence completed with critical error status");
                            }
                        } else {
                            // Фенс еще выполняется на GPU
                            nextFenceBuffer.push_back(fenceBuffer[i]);
                            nextCallbackBuffer.push_back(std::move(callbackBuffer[i]));
                        }
                    }
                }

                // ФАЗА 2: Забираем новичков из lock-free очереди
                while (m_taskQueue.try_dequeue(newTask)) {
                    vk::Result instantRes = vk::Result::eNotReady;
                    instantRes = m_device.getFenceStatus(newTask.fence);

                    if (instantRes != vk::Result::eNotReady) {
                        if (newTask.callback) {
                            newTask.callback(instantRes);
                        }
                        if (instantRes == vk::Result::eSuccess) {
                            recycleFence(newTask.fence);
                        } else {
                            vk::UniqueFence deadFence;
                            m_activeFencesTracker.try_dequeue(deadFence);
                            handleInternalError(instantRes, "Instant task check failed with critical error");
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
        }

        vk::Device m_device;
        std::atomic<bool> m_running;
        std::thread m_workerThread;

        moodycamel::ConcurrentQueue<FenceWaitTask> m_taskQueue;
        moodycamel::ConcurrentQueue<vk::UniqueFence> m_freeFences;

        // Трекер активных уникальных фенсов, чтобы не терять владение UniqueFence в рантайме
        moodycamel::ConcurrentQueue<vk::UniqueFence> m_activeFencesTracker;

        ErrorCallback m_errorCallback;
    };

} // namespace shuttle::engine
