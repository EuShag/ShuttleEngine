//
// Created by Shagu on 16.10.2026.
//

#ifndef SHUTTLEENGINE_THREADPOOLDISPATCHERTASK_HPP
#define SHUTTLEENGINE_THREADPOOLDISPATCHERTASK_HPP
#include <coroutine>
#include <utility>

#include <async/error.hpp>
#include <async/mi_allocator.hpp>

namespace shuttle::async {
    class ThreadPoolDispatcherTask {
    public:
        struct promise_type {
            // 1. Стандартный контракт C++20
            ThreadPoolDispatcherTask get_return_object() noexcept;
            static ThreadPoolDispatcherTask get_return_object_on_allocation_failure() noexcept;

            // 2. ВОТ ОНА — ВАША ИДЕЯ: Сохраняем хэндл на фазе initial_suspend!
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

            // Диспетчер никогда не засыпает на выходе — он уничтожается явно в workerLoop
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            [[nodiscard]] std::suspend_never final_suspend() const noexcept { return {}; }

            void return_void() const noexcept {}

            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            void unhandled_exception() const noexcept { std::terminate(); } // -fno-exceptions

            // 3. Реактивный Symmetric Transfer прыжок
            // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
            [[nodiscard]] auto yield_value(std::coroutine_handle<> nextHandle) const noexcept {
                struct YieldAwaiter {
                    std::coroutine_handle<> m_handle;

                    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
                    [[nodiscard]] bool await_ready() const noexcept { return false; }
                    [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                        return m_handle;
                    }
                    void await_resume() const noexcept {}
                };
                return YieldAwaiter{nextHandle};
            }

            // Ваша кастомная lock-free аллокация на MiAllocator
            void* operator new(std::size_t size) noexcept { return MiAllocator<char>().allocate(size); }
            void operator delete(void* ptr) noexcept { MiAllocator<char>().deallocate(static_cast<char*>(ptr)); }
        };

        operator bool() const noexcept { return result.has_value(); }
        [[nodiscard]] bool has_value() const noexcept { return result.has_value(); }
        [[nodiscard]] Error error() const noexcept { return result.error(); }
        operator std::coroutine_handle<>() const noexcept { return result.value(); }

        ThreadPoolDispatcherTask(std::coroutine_handle<> handle) noexcept : result(handle) {}
        ThreadPoolDispatcherTask(Error error) noexcept : result{std::unexpected{error}} {}

    private:
        Result<std::coroutine_handle<>> result;
    };
}

#endif //SHUTTLEENGINE_THREADPOOLDISPATCHERTASK_HPP
