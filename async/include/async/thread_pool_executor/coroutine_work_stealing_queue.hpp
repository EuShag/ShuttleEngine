#pragma once

#include <atomic>
#include <coroutine>
#include <cstdint>

#include <async/mi_allocator.hpp>

namespace shuttle::async {
    template <size_t LogSize = 12> // 4096 элемента по умолчанию — золотой стандарт
    class CoroutineWorkStealingQueue {
        static_assert(LogSize > 0 && LogSize < 31);

        static constexpr size_t BufferSize = size_t{1} << LogSize;
        static constexpr size_t BufferMask = BufferSize - 1;

        std::atomic<int64_t> _top {0};
        std::atomic<int64_t> _bottom {0};

        // Сырой массив атомарных хэндлов — никаких оберток!
        std::atomic<std::coroutine_handle<>> m_buffer[BufferSize];

    public:
        CoroutineWorkStealingQueue() = default;
        CoroutineWorkStealingQueue(const CoroutineWorkStealingQueue&) = delete;
        CoroutineWorkStealingQueue& operator = (const CoroutineWorkStealingQueue&) = delete;
        CoroutineWorkStealingQueue(CoroutineWorkStealingQueue&&) = default;
        CoroutineWorkStealingQueue& operator = (CoroutineWorkStealingQueue&&) = default;

        static constexpr size_t capacity() noexcept {
            return BufferSize;
        }

        [[nodiscard]] size_t size() const noexcept {
            const int64_t top = _top.load(std::memory_order_relaxed);
            const int64_t bottom = _bottom.load(std::memory_order_relaxed);
            return static_cast<size_t>(bottom >= top ? bottom - top : 0);
        }

        [[nodiscard]] bool empty() const noexcept {
            return size() == 0;
        }

        bool try_push(std::coroutine_handle<> handle) noexcept {
            const int64_t bottom = _bottom.load(std::memory_order_relaxed);
            const int64_t top = _top.load(std::memory_order_acquire);

            if (static_cast<size_t>(bottom - top + 1) > BufferSize) {
                return false; // Очередь переполнена
            }

            m_buffer[bottom & BufferMask].store(handle, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_release);
            _bottom.store(bottom + 1, std::memory_order_release);
            return true;
        }

        // Возвращает сырой хэндл. Если пуста — вернет пустой хэндл (nullptr)
        std::coroutine_handle<> pop() noexcept {
            int64_t bottom = _bottom.load(std::memory_order_relaxed) - 1;
            _bottom.store(bottom, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t top = _top.load(std::memory_order_relaxed);

            if (top <= bottom) {
                auto handle = m_buffer[bottom & BufferMask].load(std::memory_order_relaxed);
                if (top == bottom) {
                    if (!_top.compare_exchange_strong(
                        top, top + 1,
                        std::memory_order_seq_cst,
                        std::memory_order_relaxed
                    )) {
                        handle = {}; // Вор украл задачу прямо перед носом хозяина
                    }
                    _bottom.store(bottom + 1, std::memory_order_relaxed);
                }
                return handle;
            }

            _bottom.store(bottom + 1, std::memory_order_relaxed);
            return {};
        }

        // Возвращает украденный хэндл. Если нечего воровать — вернет пустой хэндл (nullptr)
        std::coroutine_handle<> steal() noexcept {
            int64_t top = _top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            const int64_t bottom = _bottom.load(std::memory_order_acquire);

            if (top < bottom) {
                auto handle = m_buffer[top & BufferMask].load(std::memory_order_relaxed);
                if (!_top.compare_exchange_strong(
                    top, top + 1,
                    std::memory_order_seq_cst,
                    std::memory_order_relaxed
                )) {
                    return {}; // Другой вор успел перехватить задачу раньше
                }
                return handle;
            }

            return {};
        }
    };
}  // namespace shuttle::async
