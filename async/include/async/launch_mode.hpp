//
// Created by Shagu on 12.10.2026.
//

#ifndef SHUTTLEENGINE_LAUNCHMODE_HPP
#define SHUTTLEENGINE_LAUNCHMODE_HPP

#endif //SHUTTLEENGINE_LAUNCHMODE_HPP

namespace shuttle::async {
    class IExecutor;

    enum class LaunchMode {
        Immediate, // Запуск в текущем потоке (синхронно)
        Async,     // Асинхронный запуск в экзекьюторе (после co_await)
        Lazy       // Ленивый запуск, корутина не стартует до start() или co_await
    };

    struct LaunchOptions {
        LaunchMode mode{ LaunchMode::Async };
        IExecutor* explicitExecutor{ nullptr };
    };
} // namespace shuttle::async
