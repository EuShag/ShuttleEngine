//
// Created by Shagu on 05.10.2026.
//

#ifndef SHUTTLEENGINE_COMMANDPOOL_HPP
#define SHUTTLEENGINE_COMMANDPOOL_HPP
#include "command_buffer_recorder.hpp"
#include "command_buffer_token.hpp"
#include "concurrentqueue.h"
#include "include_vulkan.hpp"
#include <async/mi_allocator.hpp>

namespace shuttle::engine {

    using async::MiUniquePtr;
    using async::mi_make_unique_nothrow;

    class CommandPool {
    public:

        // Конструктор по умолчанию (нужен для std::array)
        CommandPool() noexcept = default;
        CommandPool(vk::Device device, vk::detail::DispatchLoaderDynamic const* disp) noexcept;

        [[nodiscard]] static vk::ResultValue<MiUniquePtr<CommandPool>> create(
            vk::Device device, uint32_t queueFamilyIndex, vk::detail::DispatchLoaderDynamic const *disp);
        [[nodiscard]] static vk::ResultValue<CommandPool *> inPlaceCreate(
            vk::Device device, uint32_t queueFamilyIndex, vk::detail::DispatchLoaderDynamic const *disp, void *rawMemory);

        // Метод явной инициализации на месте
        [[nodiscard]] vk::Result init(uint32_t queueFamilyIndex);

        [[nodiscard]] vk::Result reset() noexcept;

        // Жесткий запрет на сдвиг адреса пула в памяти!
        CommandPool(const CommandPool&) = delete;
        CommandPool& operator=(const CommandPool&) = delete;
        CommandPool(CommandPool&&) = delete;
        CommandPool& operator=(CommandPool&&) = delete;

        template<typename Func> requires std::invocable<Func, CommandBufferRecorder const&>
        [[nodiscard]] vk::ResultValue<CommandBufferToken> writeCommandBuffer(vk::CommandBufferBeginInfo beginInfo, Func&& func);

        ~CommandPool();
    private:
        vk::ResultValue<vk::CommandBuffer> getOrCreateCommandBuffer();

        vk::Device m_device{};
        vk::detail::DispatchLoaderDynamic const* m_dispatcher{};
        vk::UniqueCommandPool m_commandPool{};
        moodycamel::ConcurrentQueue<vk::CommandBuffer> m_freeCommandBuffers{};
        moodycamel::ConcurrentQueue<vk::CommandBuffer> m_freeSecondaryCommandBuffers{};
    };

    template<typename Func> requires std::invocable<Func, CommandBufferRecorder const &>
    vk::ResultValue<CommandBufferToken> CommandPool::writeCommandBuffer(vk::CommandBufferBeginInfo beginInfo, Func &&func) {
        auto [result, commandBuffer] = getOrCreateCommandBuffer();
        if (result != vk::Result::eSuccess) {
            return {result, CommandBufferToken{}};
        }
        CommandBufferToken commandBufferToken{commandBuffer, &m_freeCommandBuffers};
        // Создаем объект CommandBufferRecorder для записи команд
        CommandBufferRecorder recorder{commandBuffer, m_dispatcher};

        // Сбрасываем командный буфер перед началом записи
        result = commandBuffer.reset({}, *m_dispatcher);
        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // Начинаем запись команд в командный буфер
        result = commandBuffer.begin(beginInfo, *m_dispatcher);
        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }
        // Вызываем пользовательскую функцию для записи команд
        func(recorder);
        // Завершаем запись команд в командный буфер
        result = commandBuffer.end(*m_dispatcher);
        if (result != vk::Result::eSuccess) {
            return {result, {}};
        }
        // Возвращаем токен командного буфера
        return {vk::Result::eSuccess, std::move(commandBufferToken)};
    }
}

#endif //SHUTTLEENGINE_COMMANDPOOL_HPP
