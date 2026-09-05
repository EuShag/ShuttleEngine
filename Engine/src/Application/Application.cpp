/**
 * @file Application.cpp
 * @brief Implementation of the main Shuttle Engine application.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Application.hpp"

#include "../../Assets/EnvironmentCompiler/include/Assets/EnvironmentCompiler/CpuIblGenerator.hpp"
#include "../../Assets/EnvironmentCompiler/src/CompiledEnvironmentBlobWriter.hpp"
#include "../Assets/SceneCompiler/src/Serialization/CompiledSceneBlobWriter.hpp"
#include "Assets/EnvironmentCompiler/EnvironmentCompiler.hpp"
#include "Assets/TextureCompiler/CompiledTexture.hpp"
#include "VulkanDebugger/VulkanDebugger.hpp"

namespace shuttle::engine::render
{

    namespace {
        struct InMemorySceneData
        {
            std::vector<assets::formats::texture::TextureMetadata> textureMetadatas;
            std::vector<assets::formats::texture::TextureMipMetadata> textureMipMetadatas;
            std::vector<uint8_t> textureBytes;
            LoadedSceneData loadedSceneData;
        };

        InMemorySceneData prepareInMemorySceneUpload(
            const assets::scene_compiler::CompiledScene& scene)
        {
            InMemorySceneData result{};
            uint64_t globalDataOffset = 0;
            uint64_t globalMipOffset = 0;

            result.textureMetadatas.reserve(scene.textures.size());

            for (const auto& texture : scene.textures)
            {
                auto metadata = texture.metadata;
                metadata.mipTableOffset = globalMipOffset;
                result.textureMetadatas.push_back(metadata);

                for (auto mip : texture.mipMetadata)
                {
                    mip.dataOffset += globalDataOffset;
                    result.textureMipMetadatas.push_back(mip);
                }

                globalMipOffset += texture.mipMetadata.size() *
                                   sizeof(assets::formats::texture::TextureMipMetadata);
                result.textureBytes.insert(
                    result.textureBytes.end(),
                    texture.data.begin(),
                    texture.data.end());
                globalDataOffset += texture.data.size();
            }

            result.loadedSceneData = LoadedSceneData{
                .nodes = scene.nodes,
                .levels = scene.levels,
                .transforms = scene.transforms,
                .drawables = scene.drawableObjects,
                .directionalLights = scene.directionalLights,
                .positions = scene.positions,
                .attributes = scene.attributes,
                .indices = scene.indices,
                .meshes = scene.meshes,
                .materials = scene.materials,
                .textureMetadatas = result.textureMetadatas,
                .textureMipMetadatas = result.textureMipMetadatas,
                .textureBytes = result.textureBytes
            };

            return result;
        }
    }

} // namespace shuttle::engine::render

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace shuttle::engine
{

    Application::Application(int argc, char** argv)
        : m_windowHandle(m_platform.createWindow(
            "Shuttle Engine", 1280, 720,
            pal::WindowType::Main,
            pal::WindowDecorationFlags::Default))
        , m_window(m_platform, m_windowHandle, "Shuttle Engine", 1280, 720)
        , m_camera(glm::vec3{10.0f, 30.3f, 0.0f})
        , m_cameraController(m_camera)
        , m_engine(m_platform, m_windowHandle, m_window)
    {
        // Регистрируем Application как слушатель событий окна и ввода
        m_window.setWindowListener(this);
        m_window.setInputListener(this);

        render::MainPassSettings mainPassSettings{};

        m_mainWindow = editor::core::MainWindow(&m_window, mainPassSettings, false);

        m_mainWindow.setOpenSceneCallback(
            [this](std::filesystem::path const& path)
            {
                loadScene(path);
            });

        m_mainWindow.setOpenEnvironmentCallback(
            [this](std::filesystem::path const& path)
            {
                loadEnvironment(path);
            });

        m_mainWindow.setImportSceneCallback(
            [this](
                std::filesystem::path const& inputPath,
                assets::scene_compiler::SceneCompilerOptions const& options)
            {
                m_engine.setHasFrameResources(false);
                if (auto waitForResult = m_engine.waitRenderIdle();
                    waitForResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error(
                        "Failed to wait for render idle: " +
                        std::to_string(static_cast<int>(waitForResult)));
                }

                auto compiledSceneOpt =
                    shuttle::assets::scene_compiler::SceneCompiler::compile(inputPath, options);

                if (!compiledSceneOpt)
                {
                    std::cerr << "[Editor] Failed to compile imported asset into memory.\n";
                    return;
                }

                auto inMemoryData =
                    render::prepareInMemorySceneUpload(*compiledSceneOpt);

                auto [uploadResult, uploadOutput] = m_engine.uploadScene(
                    inMemoryData.loadedSceneData
                );

                if (uploadResult != vk::Result::eSuccess)
                {
                    return;
                }

                OpenAsset newAsset{
                    .id = m_nextId++,
                    .name = inputPath.stem().string() + " [Unsaved]",
                    .path = inputPath,
                    .isScene = true,
                    .isDirty = true,
                    .sceneGpuData = std::move(uploadOutput),
                    .compiledSceneRAM = std::move(*compiledSceneOpt)};

                m_mainWindow.addAsset(editor::core::LoadedAsset{
                    .name = newAsset.name,
                    .path = newAsset.path,
                    .isDirty = true,
                    .id = newAsset.id,
                    .type = editor::core::AssetType::Scene,
                    .isScene = true});

                m_openAssets.push_back(std::move(newAsset));
                m_activeSceneId = m_openAssets.back().id;

            });

        m_mainWindow.setImportEnvironmentCallback(
            [this](
                std::filesystem::path const& inputPath,
                ibl::IblGenerationSettings const& settings)
            {
                m_engine.setHasFrameResources(false);
                if (auto waitForResult = m_engine.waitRenderIdle();
                    waitForResult != vk::Result::eSuccess)
                {
                    throw std::runtime_error(
                        "Failed to wait for render idle: " +
                        std::to_string(static_cast<int>(waitForResult)));
                }

                auto compiledEnvOpt =
                    assets::environment_compiler::EnvironmentCompiler::compile(
                        inputPath,
                        settings);

                if (!compiledEnvOpt)
                {
                    std::cerr << "[Editor] Failed to compile imported environment into memory.\n";
                    return;
                }

                std::filesystem::path outputPath = inputPath;
                outputPath.replace_extension(".env");

                if (!assets::environment_compiler::CompiledEnvironmentBlobWriter::write(
                        *compiledEnvOpt,
                        outputPath))
                {
                    throw std::runtime_error(
                        "Failed to write compiled environment to disk: " +
                        outputPath.string());
                }

                auto [createEnvironmentResourcesResult, environmentResources_] =
                    m_engine.createEnvironmentResources(outputPath);

                OpenAsset newAsset{
                    .id = m_nextId++,
                    .name = inputPath.stem().string() + " [Unsaved]",
                    .path = inputPath,
                    .isScene = false,
                    .isDirty = true,
                    .envGpuData = std::move(environmentResources_),
                    .compiledEnvRAM = std::move(*compiledEnvOpt)
                };

                m_mainWindow.addAsset(editor::core::LoadedAsset{
                    .name = newAsset.name,
                    .path = newAsset.path,
                    .isDirty = true,
                    .id = newAsset.id,
                    .type = editor::core::AssetType::Environment,
                    .isScene = false});

                m_openAssets.push_back(std::move(newAsset));
                m_activeEnvironmentId = m_openAssets.back().id;

            });

        m_mainWindow.setSaveSceneCallback(
            [this](std::filesystem::path const& savePath)
            {
                if (m_activeSceneId != 0)
                {
                    auto it = std::ranges::find_if(
                        m_openAssets, [&](const OpenAsset& a)
                        {
                            return a.id == m_activeSceneId && a.isScene;
                        });

                    if (it != m_openAssets.end() && it->compiledSceneRAM.has_value())
                    {
                        bool success =
                            assets::scene_compiler::CompiledSceneBlobWriter::write(
                                *it->compiledSceneRAM,
                                savePath);

                        if (success)
                        {
                            it->isDirty = false;
                            it->name = savePath.filename().string();
                            it->path = savePath;
                            m_mainWindow.markAssetSaved(it->id, it->name, it->path);
                            std::cout << "[Editor] Asset successfully saved to disk: "
                                      << savePath << '\n';
                        }
                    }
                }
            });

        m_mainWindow.setSaveEnvironmentCallback(
            [this](std::filesystem::path const& savePath)
            {
                if (m_activeEnvironmentId != 0)
                {
                    auto it = std::ranges::find_if(
                        m_openAssets, [&](const OpenAsset& a) {
                            return a.id == m_activeEnvironmentId && !a.isScene;
                        });

                    if (it != m_openAssets.end() && it->compiledEnvRAM.has_value())
                    {
                        bool success =
                            shuttle::assets::environment_compiler::CompiledEnvironmentBlobWriter::write(
                                *it->compiledEnvRAM,
                                savePath);

                        if (success)
                        {
                            it->isDirty = false;
                            it->name = savePath.filename().string();
                            it->path = savePath;
                            m_mainWindow.markAssetSaved(it->id, it->name, it->path);
                            std::cout << "[Editor] Environment asset successfully saved to disk: "
                                      << savePath << '\n';
                        }
                        else
                        {
                            std::cerr << "[Editor] Failed to save environment asset to disk: "
                                      << savePath << '\n';
                        }
                    }
                }
            });

        m_mainWindow.setSelectAssetCallback(
            [this](editor::core::ResourceId id)
            {
                auto it = std::ranges::find_if(
                    m_openAssets, [id](const OpenAsset& a) {
                        return a.id == id;
                    });

                if (it != m_openAssets.end())
                {
                    if (it->isScene)
                    {
                        m_activeSceneId = id;
                        m_engine.setHasFrameResources(false);
                        if (auto waitForResult = m_engine.waitRenderIdle();
                            waitForResult != vk::Result::eSuccess)
                        {
                            throw std::runtime_error(
                                "Failed to wait for render idle: " +
                                std::to_string(static_cast<int>(waitForResult)));
                        }
                    }
                    else
                    {
                        m_activeEnvironmentId = id;
                    }
                }
            });

        m_mainWindow.setCloseAssetCallback(
            [this](editor::core::ResourceId id)
            {
                if (auto result = m_engine.waitRenderIdle();
                    result != vk::Result::eSuccess)
                {
                    std::cerr << "Failed to wait for render idle: "
                              << static_cast<int>(result)
                              << '\n';
                }

                auto it = std::ranges::find_if(
                    m_openAssets, [id](const OpenAsset& asset) {
                        return asset.id == id;
                    });

                if (it == m_openAssets.end())
                {
                    return;
                }

                if (it->isScene) {
                    if (m_activeSceneId == id) {
                        m_activeSceneId = 0;
                        m_engine.setHasFrameResources(false);
                    }
                }
                else {
                    if (m_activeEnvironmentId == id) {
                        m_activeEnvironmentId = 0;
                    }
                }

                std::erase_if(
                    m_openAssets, [id](const OpenAsset& asset) {
                        return asset.id == id;
                    });

                m_engine.setHasFrameResources(false);
            });

        m_lastTime = std::chrono::high_resolution_clock::now();

        // Показываем окно после завершения всей инициализации
        m_window.show();
    }

    // ---------------------------------------------------------------------
    // ОБРАБОТЧИКИ СОБЫТИЙ СЛУШАТЕЛЕЙ (PAL Event Listeners)
    // ---------------------------------------------------------------------

    void Application::onWindowResize(const pal::WindowResizeEvent& event)
    {
        // При изменении размеров нативного окна
    }

    void Application::onWindowCloseRequested()
    {
        std::cout << "[System] Window close event received, posting quit...\n";
        m_platform.postQuitEvent();
    }

    void Application::onWindowPaint() {
        auto scene = findActiveScene();
        auto environment = findActiveEnvironment();
        m_engine.drawFrame(true, m_deltaTime, m_window, m_mainWindow, m_camera, scene, environment);
    }

    void Application::onKeyboard(const input::KeyboardEvent& event)
    {
            if (m_mainWindow.isViewportFocused()) {
                m_cameraController.onKeyboard(event);
            }
            if (event.state == pal::KeyState::Pressed && event.key == pal::KeyCode::Escape) {
                std::cout << "[System] Escape key pressed, closing application...\n";
                m_platform.postQuitEvent();
            }
    }

    const render::UploadSceneOutput* Application::findActiveScene() const
    {
        if (m_activeSceneId != 0)
        {
            auto it = std::ranges::find_if(
                m_openAssets, [&](const OpenAsset& a) {
                    return a.id == m_activeSceneId && a.isScene;
                });

            if (it != m_openAssets.end() && it->sceneGpuData.has_value())
            {
                return &it->sceneGpuData.value();
            }
        }

        for (const auto& asset : m_openAssets)
        {
            if (asset.isScene && asset.sceneGpuData.has_value())
            {
                return &asset.sceneGpuData.value();
            }
        }

        return nullptr;
    }

    const render::DeviceEnvironmentResources* Application::findActiveEnvironment() const
    {
        if (m_activeEnvironmentId != 0)
        {
            auto const it = std::ranges::find_if(
                m_openAssets, [&](const OpenAsset& a) {
                    return a.id == m_activeEnvironmentId && !a.isScene;
                });

            if (it != m_openAssets.end() && it->envGpuData.has_value())
            {
                return &it->envGpuData.value();
            }
        }

        for (const auto& asset : m_openAssets)
        {
            if (!asset.isScene && asset.envGpuData.has_value())
            {
                return &asset.envGpuData.value();
            }
        }

        return nullptr;
    }

    void Application::loadScene(std::filesystem::path const& path)
    {
        m_engine.setHasFrameResources(false);
        if (auto waitForResult = m_engine.waitRenderIdle();
            waitForResult != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Failed to wait for render idle: " +
                std::to_string(static_cast<int>(waitForResult)));
        }

        auto loadedSceneData = render::loadSceneData(path);
        if (loadedSceneData.result != vk::Result::eSuccess)
        {
            return;
        }

        auto [uploadSceneResult, uploadSceneOutput] = m_engine.uploadScene(*loadedSceneData);

        if (uploadSceneResult != vk::Result::eSuccess)
        {
            return;
        }

        OpenAsset item{
            .id = m_nextId++,
            .name = path.filename().string(),
            .path = path,
            .isScene = true,
            .isDirty = false,
            .sceneGpuData = std::move(uploadSceneOutput)};

        m_mainWindow.addAsset(editor::core::LoadedAsset{
            .name = item.name,
            .path = item.path,
            .isDirty = false,
            .id = item.id,
            .type = shuttle::editor::core::AssetType::Scene,
            .isScene = true});

        m_openAssets.push_back(std::move(item));
        m_activeSceneId = m_openAssets.back().id;

    }

    void Application::loadEnvironment(std::filesystem::path const& path)
    {
        m_engine.setHasFrameResources(false);
        if (auto waitForResult = m_engine.waitRenderIdle();
            waitForResult != vk::Result::eSuccess)
        {
            throw std::runtime_error(
                "Failed to wait for render idle: " +
                std::to_string(static_cast<int>(waitForResult)));
        }

        auto [createEnvironmentResourcesResult, environmentResources_] =
            m_engine.createEnvironmentResources(path);

        if (createEnvironmentResourcesResult != vk::Result::eSuccess)
        {
            return;
        }

        OpenAsset item{
            .id = m_nextId++,
            .name = path.filename().string(),
            .path = path,
            .isScene = false,
            .isDirty = false,
            .envGpuData = std::move(environmentResources_)};

        m_mainWindow.addAsset(editor::core::LoadedAsset{
            .name = item.name,
            .path = item.path,
            .isDirty = false,
            .id = item.id,
            .type = editor::core::AssetType::Environment,
            .isScene = false});

        m_openAssets.push_back(std::move(item));
        m_activeEnvironmentId = m_openAssets.back().id;
    }

    int Application::run()
    {
        std::cout << "[Run] Entering main render loop. Engine is green.\n";

        while (!m_platform.shouldQuit() && !m_window.shouldClose())
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            m_deltaTime = std::chrono::duration<float>(currentTime - m_lastTime).count();
            m_lastTime = currentTime;
            m_totalTime += m_deltaTime;

            if (!m_platform.pollEvents()) break;
            if (m_window.isMinimized()) continue;

            m_cameraController.update(
                m_deltaTime, m_mainWindow.getCameraMoveSpeed(),
                m_mainWindow.getCameraRotationSpeed());

            m_engine.drawFrame(false, m_deltaTime, m_window, m_mainWindow, m_camera,
                findActiveScene(), findActiveEnvironment());
        }

        return 0;
    }

} // namespace shuttle::engine
