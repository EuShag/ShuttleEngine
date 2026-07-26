#pragma once

#include <array>
#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <vector>

enum class ToolMode
{
    Scene,
    Environment
};

class AssetProcessorApp
{
public:
    void draw();

private:
    //
    // UI
    //

    void drawToolbar();
    void drawPaths();
    void drawActions();
    void drawLog();

    //
    // dialogs
    //

    void browseSource();
    void browseDestination();

    //
    // jobs
    //

    void compileScene();
    void compileEnvironment();

    //
    // logging
    //

    void pushLog(std::string text);

private:
    ToolMode m_mode =
        ToolMode::Scene;

    std::array<char, 1024>
        m_source{};

    std::array<char, 1024>
        m_destination{};

    std::mutex
        m_logMutex;

    std::vector<std::string>
        m_logs;

    std::future<void>
        m_worker;

    std::atomic_bool
        m_busy = false;
};