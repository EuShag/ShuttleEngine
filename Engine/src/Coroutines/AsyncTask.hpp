#pragma once

#include <coroutine>
#include <atomic>
#include <utility>
#include <expected>
#include <memory>
#include "CoroScope.hpp"
#include "Awaiters.hpp"
// Предполагаем наличие твоих noexcept оберток под mimalloc
#include "../MiAllocator/MiAllocator.hpp"

namespace shuttle::engine {

    template<typename T = void>
    class AsyncTask {
    public:
        class promise_type {
        public:
            template<typename TObj, typename ... TArgs>
            promise_type(TObj&&, CoroScope& scope, TArgs&...) {
                m_coroScope = &scope;
            }
            template<typename ... TArgs>
            promise_type(CoroScope& scope, TArgs&...) {
                m_coroScope = &scope;
            }

            [[nodiscard]] std::expected<AsyncTask, ResultCode> get_return_object() const noexcept {
                auto coroHandle = std::coroutine_handle<promise_type>::from_promise(*this);
                m_coroScope->registerCoroHandle(coroHandle);
                return AsyncTask{m_promise.get_future(), coroHandle};
            }
            [[nodiscard]] static std::expected<AsyncTask, ResultCode> get_return_on_allocation_failure() noexcept {
                return std::unexpected{ResultCode::ALLOCATION_FAILURE};
            }
            [[nodiscard]] auto initial_suspend() const noexcept { return InitialAwaiter{m_coroScope}; }
            [[nodiscard]] auto final_suspend() const noexcept { return FinalAwaiter{m_coroScope}; }
            void unhandled_exception() noexcept { m_promise.set_exception(std::current_exception()); }
            void cancel() noexcept { m_isCanceled.store(true, std::memory_order_relaxed); }

            // Этот метод материализуется ТОЛЬКО если T — это void
            void return_void() noexcept requires std::same_as<T, void> {
                m_promise.set_value(); // У std::promise<void> метод set_value() не принимает аргументов
                m_coroScope->joinTask();
            }

            // Перемещение: включается только если T — НЕ void
            void return_value(T&& value)
                noexcept(std::is_nothrow_move_constructible_v<T>)
                requires (!std::same_as<T, void>)
            {
                m_promise.set_value(std::move(value));
                m_coroScope->joinTask();
            }

            // Копирование: включается только если T — НЕ void И тип можно копировать
            void return_value(const T& value)
                noexcept(std::is_nothrow_copy_constructible_v<T>)
                requires (!std::same_as<T, void> && std::copy_constructible<T>)
            {
                m_promise.set_value(value);
                m_coroScope->joinTask();
            }

            [[nodiscard]] void* operator new(std::size_t size) noexcept { return MiAllocator<char>().allocate(size); }
            void operator delete(void* ptr) noexcept { MiAllocator<char>().deallocate(static_cast<char*>(ptr)); }

        private:
            std::atomic_uint64_t m_isCanceled{ false };
            std::promise<T> m_promise;
            CoroScope* m_coroScope;
        };

        AsyncTask(std::future<T> future, std::coroutine_handle<promise_type> handle)
            : m_future(std::move(future)), m_handle(handle) {}

    private:
        std::future<T> m_future;
        std::coroutine_handle<promise_type> m_handle;
        friend class CoroScope;
    };

} // namespace shuttle::engine
