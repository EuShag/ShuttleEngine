#pragma once
#include <concepts>
#include <utility>

namespace shuttle::async {

    /**
     * --- УЛЬТРАЛЕГКИЙ КОНТЕЙНЕР РЕЗУЛЬТАТА (Замена жирного std::optional) ---
     */
    template <typename T>
    struct AsyncResult final {
        // Храним значение по типу. Для корутин это будет T&&, передаваемый без копирования!
        T value;
        bool isCancelled{ false };

        // Удобный оператор приведения к bool для синтаксического сахара в ветвлениях logic-кадра
        constexpr explicit operator bool() const noexcept { return !isCancelled; }
    };

    /**
     * --- СПЕЦИАЛИЗАЦИЯ ДЛЯ VOID-КОРУТИН (Только флаг отмены) ---
     */
    template <>
    struct AsyncResult<void> final {
        bool isCancelled{ false };

        // true означает, что задача завершилась успешно, false — была отменена
        constexpr explicit operator bool() const noexcept { return !isCancelled; }
    };

} // namespace shuttle::async
