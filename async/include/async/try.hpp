#pragma once

#include <async/error.hpp>

#include <utility>

#define ASYNC_TRY_VOID(expression)                                           \
    do {                                                                     \
        auto _async_result = (expression);                                   \
        if (!_async_result) [[unlikely]] {                                   \
            return ::std::unexpected(_async_result.error());                \
        }                                                                    \
    } while (false)

#define ASYNC_DETAIL_CONCAT_INNER(lhs, rhs) lhs##rhs
#define ASYNC_DETAIL_CONCAT(lhs, rhs) ASYNC_DETAIL_CONCAT_INNER(lhs, rhs)
#define ASYNC_DETAIL_TRY(variable, expression, result_name)                  \
    auto result_name = (expression);                                         \
    if (!result_name) [[unlikely]] {                                         \
        return ::std::unexpected(result_name.error());                      \
    }                                                                        \
    auto variable = std::move(result_name).value()

#define ASYNC_TRY(variable, expression)                                       \
    ASYNC_DETAIL_TRY(variable, expression, ASYNC_DETAIL_CONCAT(_async_result_, variable))
