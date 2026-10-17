//
// Created by Shagu on 05.10.2026.
//

#ifndef SHUTTLEENGINE_COMMAND_BUFFER_TOKEN_HPP
#define SHUTTLEENGINE_COMMAND_BUFFER_TOKEN_HPP

#include "include_vulkan.hpp"
#include "concurrentqueue.h"

namespace shuttle::engine {
    class CommandBufferToken {
        vk::CommandBuffer m_commandBuffer;
        moodycamel::ConcurrentQueue<vk::CommandBuffer>* m_returnQueue = nullptr;
    public:
        CommandBufferToken(vk::CommandBuffer cb, moodycamel::ConcurrentQueue<vk::CommandBuffer>* returnQueue);

        CommandBufferToken(const CommandBufferToken&) = delete;
        CommandBufferToken& operator=(const CommandBufferToken&) = delete;

        CommandBufferToken(CommandBufferToken&& other) noexcept;

        CommandBufferToken& operator=(CommandBufferToken&& other) noexcept;

        CommandBufferToken() = default;

        ~CommandBufferToken();
    };
}

#endif //SHUTTLEENGINE_COMMAND_BUFFER_TOKEN_HPP
