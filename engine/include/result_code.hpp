//
// Created by Shagu on 13.10.2026.
//

#ifndef SHUTTLEENGINE_ERRORCODE_HPP
#define SHUTTLEENGINE_ERRORCODE_HPP

#include <cstdint>
#include <expected>
#include <utility>

namespace shuttle {

    // 1. Общие категории подсистем движка
    enum class ErrorCategory : uint8_t {
        SUCCESS = 0,
        COMMON,       // Общие системные сбои
        EXECUTOR      // Специфичные ошибки планировщика корутин
    };

    // 2. Плоский монолитный enum кодов в стиле Vulkan API / Win32
    enum class [[nodiscard]] ResultCode : int32_t {
        // --- Успешные статусы (>= 0) ---
        SUCCESS                  = 0,
        NOT_READY                = 1,
        TIMEOUT                  = 2,

        // --- Общие системные ошибки (< 0) ---
        ALLOCATION_FAILURE       = -1,
        UNEXPECTED_FAILURE       = -2,

        // --- Специфичные ошибки Планировщика (Диапазон от -100 до -199) ---
        EXECUTOR_INVALID_WORKER_COUNT     = -100,
        EXECUTOR_WORKER_START_FAILED      = -101,
        EXECUTOR_WORKER_JOIN_FAILED       = -102,
        EXECUTOR_INVALID_COROUTINE_HANDLE = -103,
        EXECUTOR_STOPPED                  = -104,
        EXECUTOR_QUEUE_ENQUEUE_FAILED     = -105,
        EXECUTOR_NOT_EXECUTOR_WORKER      = -106,
        EXECUTOR_QUEUE_OVERFLOW           = -107
    };

    // Быстрые инлайн-хелперы для проверки статуса
    [[nodiscard]] inline constexpr bool is_success(ResultCode res) noexcept {
        return static_cast<int32_t>(res) >= 0;
    }

    [[nodiscard]] inline constexpr bool is_error(ResultCode res) noexcept {
        return static_cast<int32_t>(res) < 0;
    }

    // 3. Вычисление категории ошибки "на лету" без полиморфизма и мьютексов
    [[nodiscard]] inline constexpr ErrorCategory get_error_category(ResultCode res) noexcept {
        const int32_t val = static_cast<int32_t>(res);

        if (val >= 0) {
            return ErrorCategory::SUCCESS;
        }

        // Проверяем диапазон планировщика корутин
        if (val <= -100 && val >= -199) {
            return ErrorCategory::EXECUTOR;
        }

        // Все остальные отрицательные значения относятся к общим сбоям
        return ErrorCategory::COMMON;
    }

} // namespace shuttle

// =================================================================
// 1. ГРУППА VOID: ДЛЯ ФУНКЦИЙ, ВОЗВРАЩАЮЩИХ ПРОСТО ResultCode
// =================================================================

// Базовая проверка с авто-return
#define SHUTTLE_TRY_VOID(expr)                                             \
    do {                                                                   \
        const ::shuttle::ResultCode _shuttle_res = (expr);                 \
        if (::shuttle::is_error(_shuttle_res)) [[unlikely]] {              \
            return _shuttle_res;                                           \
        }                                                                  \
    } while (false)

// Базовая проверка для корутин с авто-co_return
#define SHUTTLE_CO_TRY_VOID(expr)                                          \
    do {                                                                   \
        const ::shuttle::ResultCode _shuttle_res = (expr);                 \
        if (::shuttle::is_error(_shuttle_res)) [[unlikely]] {              \
            co_return _shuttle_res;                                        \
        }                                                                  \
    } while (false)

// Гибкая проверка с КАСТОМНЫМ ИМЕНЕМ переменной ошибки (err_var)
#define SHUTTLE_CHECK_ACTION_ERR(expr, err_var, err_action)                \
    do {                                                                   \
        const ::shuttle::ResultCode err_var = (expr);                      \
        if (::shuttle::is_error(err_var)) [[unlikely]] {                   \
            err_action;                                                    \
        }                                                                  \
    } while (false)

// То же самое для корутин (разрешает co_return/co_await в action)
#define SHUTTLE_CHECK_CO_ACTION_ERR(expr, err_var, err_action)             \
    do {                                                                   \
        const ::shuttle::ResultCode err_var = (expr);                      \
        if (::shuttle::is_error(err_var)) [[unlikely]] {                   \
            err_action;                                                    \
        }                                                                  \
    } while (false)

// =================================================================
// 2. ГРУППА VALUE: ДЛЯ ФУНКЦИЙ, ВОЗВРАЩАЮЩИХ std::expected<T, ResultCode>
// =================================================================

// Базовая распаковка с авто-return (Защищено через else)
#define SHUTTLE_TRY(var, expr)                                             \
    auto&& _shuttle_expected_##var = (expr);                               \
    if (_shuttle_expected_##var.has_value()) [[likely]] {} else            \
        return ::std::unexpected(_shuttle_expected_##var.error());         \
    auto var = ::std::move(_shuttle_expected_##var.value())

// Базовая распаковка для корутин с авто-co_return
#define SHUTTLE_CO_TRY(var, expr)                                          \
    auto&& _shuttle_expected_##var = (expr);                               \
    if (_shuttle_expected_##var.has_value()) [[likely]] {} else            \
        co_return ::std::unexpected(_shuttle_expected_##var.error());      \
    auto var = ::std::move(_shuttle_expected_##var.value())

// Гибкая распаковка с КАСТОМНЫМ ИМЕНЕМ переменной ошибки (err_var)
#define SHUTTLE_TRY_ACTION_ERR(var, expr, err_var, err_action)             \
    auto&& _shuttle_exp_##var = (expr);                                    \
    if (_shuttle_exp_##var.has_value()) [[likely]] {} else {               \
        const ::shuttle::ResultCode err_var = _shuttle_exp_##var.error();   \
        err_action;                                                        \
    }                                                                      \
    auto var = ::std::move(_shuttle_exp_##var.value())

// То же самое для корутин (разрешает co_return/co_await в action)
#define SHUTTLE_CO_TRY_ACTION_ERR(var, expr, err_var, err_action)          \
    auto&& _shuttle_exp_##var = (expr);                                    \
    if (_shuttle_exp_##var.has_value()) [[likely]] {} else {               \
        const ::shuttle::ResultCode err_var = _shuttle_exp_##var.error();   \
        err_action;                                                        \
    }                                                                      \
    auto var = ::std::move(_shuttle_exp_##var.value())

#endif //SHUTTLEENGINE_ERRORCODE_HPP
