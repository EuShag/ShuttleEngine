//
// Created by Shagu on 12.10.2026.
//

#ifndef SHUTTLEENGINE_AWAITERS_HPP
#define SHUTTLEENGINE_AWAITERS_HPP
#include <coroutine>

namespace shuttle::async {

    class CoroScope;

    class InitialAwaiter {
    public:
        // Передаем хэндл родителя, который нужно разбудить/продолжить
        explicit InitialAwaiter(CoroScope* scope) noexcept : coroScope{scope} {}

        [[nodiscard]] bool await_ready() const noexcept;

        // Возвращаем хэндл корутины, которой нужно ОТДАТЬ управление на текущем потоке
        [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) const noexcept;

        void await_resume() const noexcept {};

    private:
        CoroScope *coroScope{ nullptr };
    };

    class FinalAwaiter {
    public:
        explicit FinalAwaiter(CoroScope* scope) noexcept : m_scope(scope) {}

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) const noexcept;

        void await_resume() const noexcept {}

    private:
        CoroScope* m_scope{ nullptr };
    };


}
#endif //SHUTTLEENGINE_AWAITERS_HPP