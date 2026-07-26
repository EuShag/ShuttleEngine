#include "AssetProcessorApp.hpp"

#include <cstring>
#include <filesystem>
#include <thread>

#include <imgui.h>

#include <Assets/SceneCompiler/SceneCompiler.hpp>

#include <Assets/EnvironmentCompiler/EnvironmentCompiler.hpp>

#include "CompiledEnvironmentBlobWriter.hpp"
#include "portable-file-dialogs.h"
#include "Serialization/CompiledSceneBlobWriter.hpp"

void AssetProcessorApp::draw()
{
    ImGui::Begin(
        "Shuttle Engine Asset Processor",
        nullptr,
        ImGuiWindowFlags_NoCollapse);

    drawToolbar();

    ImGui::Separator();

    drawPaths();

    ImGui::Separator();

    drawActions();

    ImGui::Separator();

    drawLog();

    ImGui::End();
}

void AssetProcessorApp::drawToolbar()
{
    if (ImGui::RadioButton(
            "Scene",
            m_mode == ToolMode::Scene))
    {
        m_mode = ToolMode::Scene;
    }

    ImGui::SameLine();

    if (ImGui::RadioButton(
            "Environment",
            m_mode == ToolMode::Environment))
    {
        m_mode = ToolMode::Environment;
    }
}

void AssetProcessorApp::drawPaths()
{
    ImGui::Text("Source");

    ImGui::InputText(
        "##Source",
        m_source.data(),
        m_source.size());

    ImGui::SameLine();

    if (ImGui::Button(
            "Browse##Source"))
    {
        browseSource();
    }

    ImGui::Spacing();

    ImGui::Text("Destination");

    ImGui::InputText(
        "##Destination",
        m_destination.data(),
        m_destination.size());

    ImGui::SameLine();

    if (ImGui::Button(
            "Browse##Destination"))
    {
        browseDestination();
    }
}

void AssetProcessorApp::drawActions()
{
    if (m_busy)
    {
        ImGui::BeginDisabled();
    }

    if (m_mode == ToolMode::Scene)
    {
        if (ImGui::Button(
                "Compile Scene",
                ImVec2(220.0f, 40.0f)))
        {
            compileScene();
        }
    }
    else
    {
        if (ImGui::Button(
                "Compile Environment",
                ImVec2(220.0f, 40.0f)))
        {
            compileEnvironment();
        }
    }

    if (m_busy)
    {
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::Text("Working...");
    }
}

void AssetProcessorApp::drawLog()
{
    ImGui::Text("Log");

    ImGui::BeginChild(
        "LogWindow",
        ImVec2(0, 300),
        true);

    std::scoped_lock lock(
        m_logMutex);

    for (const auto& line : m_logs)
    {
        ImGui::TextUnformatted(
            line.c_str());
    }

    ImGui::EndChild();
}

void AssetProcessorApp::browseSource()
{
    auto files =
        pfd::open_file(
            "Select Source",
            "",
            {
                "All Assets",
                "*.fbx *.obj *.gltf *.glb *.hdr",

                "Scene Files",
                "*.fbx *.obj *.gltf *.glb",

                "HDR Files",
                "*.hdr"
            }).result();

    if (files.empty())
    {
        return;
    }

    std::strncpy(
        m_source.data(),
        files[0].c_str(),
        m_source.size() - 1);

    namespace fs = std::filesystem;

    fs::path output =
        fs::path(files[0]);

    output.replace_extension(
        ".sblb");

    const std::string out =
        output.string();

    std::strncpy(
        m_destination.data(),
        out.c_str(),
        m_destination.size() - 1);
}

void AssetProcessorApp::browseDestination()
{
    auto file =
        pfd::save_file(
            "Save Asset",
            "",
            {
                "Blob Files",
                "*.sblb",

                "All Files",
                "*"
            }).result();

    if (file.empty())
    {
        return;
    }

    std::strncpy(
        m_destination.data(),
        file.c_str(),
        m_destination.size() - 1);
}

void AssetProcessorApp::compileScene()
{
    if (m_busy)
    {
        return;
    }

    m_busy = true;

    const std::string source =
        m_source.data();

    const std::string destination =
        m_destination.data();

    pushLog(
        "[Scene] Compilation started.");

    m_worker =
        std::async(
            std::launch::async,
            [this, source, destination]
            {
                try
                {
                    auto scene =
                        shuttle::assets::scene_compiler::
                            SceneCompiler::compile(
                                source);

                    if (!scene)
                    {
                        pushLog(
                            "[Scene] Compiler returned null.");

                        m_busy = false;
                        return;
                    }

                    const bool success =
                        shuttle::assets::scene_compiler::
                            CompiledSceneBlobWriter::write(
                                *scene,
                                destination);

                    if (success)
                    {
                        pushLog(
                            "[Scene] Compilation completed.");
                    }
                    else
                    {
                        pushLog(
                            "[Scene] Failed to write blob.");
                    }
                }
                catch (const std::exception& exception)
                {
                    pushLog(
                        std::string(
                            "[Scene] Exception: ") +
                        exception.what());
                }
                catch (...)
                {
                    pushLog(
                        "[Scene] Unknown exception.");
                }

                m_busy = false;
            });
}

void AssetProcessorApp::compileEnvironment()
{
    if (m_busy)
    {
        return;
    }

    m_busy = true;

    const std::string source =
        m_source.data();

    const std::string destination =
        m_destination.data();

    pushLog(
        "[Environment] Compilation started.");

    m_worker =
        std::async(
            std::launch::async,
            [this, source, destination]
            {
                try
                {
                    auto environment =
                        shuttle::assets::
                            environment_compiler::
                                EnvironmentCompiler::compile(
                                    source);

                    if (!environment)
                    {
                        pushLog(
                            "[Environment] Compiler returned null.");

                        m_busy = false;
                        return;
                    }

                    const bool success =
                        shuttle::assets::
                            environment_compiler::
                                CompiledEnvironmentBlobWriter::write(
                                    *environment,
                                    destination);

                    if (success)
                    {
                        pushLog(
                            "[Environment] Compilation completed.");
                    }
                    else
                    {
                        pushLog(
                            "[Environment] Failed to write blob.");
                    }
                }
                catch (const std::exception& exception)
                {
                    pushLog(
                        std::string(
                            "[Environment] Exception: ") +
                        exception.what());
                }
                catch (...)
                {
                    pushLog(
                        "[Environment] Unknown exception.");
                }

                m_busy = false;
            });
}

void AssetProcessorApp::pushLog(
    std::string text)
{
    std::scoped_lock lock(
        m_logMutex);

    m_logs.push_back(
        std::move(text));
}