//
// Created by Shagu on 16.10.2026.
//

#include <async/thread_pool_executor/thread_pool_dispatcher_task.hpp>
namespace shuttle::async {
    ThreadPoolDispatcherTask ThreadPoolDispatcherTask::promise_type::get_return_object() noexcept {
        return { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    ThreadPoolDispatcherTask ThreadPoolDispatcherTask::promise_type::
    get_return_object_on_allocation_failure() noexcept {
        return { Error::allocation_failure };
    }
}