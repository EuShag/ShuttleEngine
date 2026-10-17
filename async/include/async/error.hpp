#pragma once

#include <expected>
#include <string_view>

namespace shuttle::async {

    enum class Error {
        allocation_failure,
        invalid_worker_count,
        worker_start_failed,
        invalid_coroutine_handle,
        executor_stopped,
        queue_enqueue_failed,
        executor_unavailable
    };

    [[nodiscard]] constexpr std::string_view to_string(Error error) noexcept {
        switch (error) {
        case Error::allocation_failure: return "allocation failure";
        case Error::invalid_worker_count: return "invalid worker count";
        case Error::worker_start_failed: return "worker start failed";
        case Error::invalid_coroutine_handle: return "invalid coroutine handle";
        case Error::executor_stopped: return "executor stopped";
        case Error::queue_enqueue_failed: return "queue enqueue failed";
        case Error::executor_unavailable: return "executor unavailable";
        }
        return "unknown async error";
    }

    template <typename T>
    using Result = std::expected<T, Error>;

    using Status = Result<void>;

}
