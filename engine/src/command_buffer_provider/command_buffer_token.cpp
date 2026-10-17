//
// Created by Shagu on 05.10.2026.
//

#include "command_buffer_token.hpp"
namespace shuttle::engine {
    CommandBufferToken::CommandBufferToken(vk::CommandBuffer cb,
        moodycamel::ConcurrentQueue<vk::CommandBuffer> *returnQueue): m_commandBuffer(cb), m_returnQueue(returnQueue) {}

    CommandBufferToken::CommandBufferToken(CommandBufferToken &&other) noexcept: m_commandBuffer(other.m_commandBuffer), m_returnQueue(other.m_returnQueue) {
        other.m_returnQueue = nullptr;
    }

    CommandBufferToken & CommandBufferToken::operator=(CommandBufferToken &&other) noexcept {
        if (this != &other) {
            if (m_returnQueue) m_returnQueue->enqueue(m_commandBuffer);
            m_commandBuffer = other.m_commandBuffer;
            m_returnQueue = other.m_returnQueue;
            other.m_returnQueue = nullptr;
        }
        return *this;
    }

    CommandBufferToken::~CommandBufferToken() {
        if (m_returnQueue) {
            // Токен умер (буфер отправили в граф/очередь) -> просто возвращаем хэндл буфера в пул.
            // Это на 100% безопасно, так как moodycamel внутри пула потокобезопасен!
            m_returnQueue->enqueue(m_commandBuffer);
        }
    }
}
