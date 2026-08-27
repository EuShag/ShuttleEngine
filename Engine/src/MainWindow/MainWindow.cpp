#include "MainWindow.hpp"

#include <algorithm>
#include <utility>

#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "portable-file-dialogs.h"

namespace shuttle::editor::core
{
    enum class WindowButtonType
    {
        Minimize,
        Maximize,
        Restore,
        Close
    };

    static bool drawSliderProperty(
        const char* label,
        float* value,
        float min,
        float max,
        float step = 0.01f,
        const char* format = "%.3f")
    {
        ImGui::PushID(label);
        ImGui::Columns(3, nullptr, false);
        ImGui::SetColumnWidth(0, 110.0f);
        ImGui::SetColumnWidth(1, 100.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        bool changed = ImGui::SliderFloat("##slider", value, min, max, "");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        changed |= ImGui::InputFloat("##input", value, step, step * 10.0f, format);
        *value = std::clamp(*value, min, max);
        ImGui::Columns(1);
        ImGui::PopID();
        return changed;
    }

    static bool drawWindowButton(
        const char* id,
        WindowButtonType type,
        ImVec2 size)
    {
        ImGui::PushID(id);
        ImVec2 position = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("Button", size);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 backgroundColor = IM_COL32(0, 0, 0, 0);

        if (type == WindowButtonType::Close)
        {
            if (active)
            {
                backgroundColor = IM_COL32(180, 40, 40, 255);
            }
            else if (hovered)
            {
                backgroundColor = IM_COL32(220, 15, 15, 255);
            }
        }
        else if (active)
        {
            backgroundColor = IM_COL32(255, 255, 255, 40);
        }
        else if (hovered)
        {
            backgroundColor = IM_COL32(255, 255, 255, 35);
        }

        drawList->AddRectFilled(
            position,
            ImVec2(position.x + size.x, position.y + size.y),
            backgroundColor);

        ImU32 iconColor = IM_COL32(255, 255, 255, 255);
        float centerX = position.x + size.x * 0.5f;
        float centerY = position.y + size.y * 0.5f;
        float iconSize = 12.0f;
        constexpr float thickness = 2.0f;

        switch (type)
        {
            case WindowButtonType::Minimize:
                drawList->AddLine(
                    ImVec2(centerX - iconSize * 0.5f, centerY + 3.0f),
                    ImVec2(centerX + iconSize * 0.5f, centerY + 3.0f),
                    iconColor,
                    thickness);
                break;

            case WindowButtonType::Maximize:
                drawList->AddRect(
                    ImVec2(centerX - iconSize * 0.5f, centerY - iconSize * 0.5f),
                    ImVec2(centerX + iconSize * 0.5f, centerY + iconSize * 0.5f),
                    iconColor,
                    1.0f,
                    thickness);
                break;

            case WindowButtonType::Restore:
            {
                float iconScale = iconSize;
                ImVec2 backTopLeft(centerX - iconScale * 0.13f, centerY - iconScale * 0.53f);
                ImVec2 backBottomRight(centerX + iconScale * 0.53f, centerY + iconScale * 0.13f);
                ImVec2 frontTopLeft(centerX - iconScale * 0.57f, centerY - iconScale * 0.17f);
                ImVec2 frontBottomRight(centerX + iconScale * 0.17f, centerY + iconScale * 0.57f);

                drawList->AddLine(
                    backTopLeft,
                    ImVec2(backBottomRight.x, backTopLeft.y),
                    iconColor,
                    thickness * 0.75f);
                drawList->AddLine(
                    ImVec2(backBottomRight.x, backTopLeft.y),
                    backBottomRight,
                    iconColor,
                    thickness * 0.75f);
                drawList->AddRect(
                    frontTopLeft,
                    frontBottomRight,
                    iconColor,
                    0.0f,
                    thickness);
                break;
            }

            case WindowButtonType::Close:
                drawList->AddLine(
                    ImVec2(centerX - iconSize * 0.45f, centerY - iconSize * 0.45f),
                    ImVec2(centerX + iconSize * 0.45f, centerY + iconSize * 0.45f),
                    iconColor,
                    thickness);
                drawList->AddLine(
                    ImVec2(centerX + iconSize * 0.45f, centerY - iconSize * 0.45f),
                    ImVec2(centerX - iconSize * 0.45f, centerY + iconSize * 0.45f),
                    iconColor,
                    thickness);
                break;
        }

        ImGui::PopID();
        return clicked;
    }

    static bool drawTitleBarButton(
        const char* id,
        const char* text,
        ImVec2 size)
    {
        ImGui::PushID(id);
        ImVec2 position = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("Button", size);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        ImU32 background = IM_COL32(0, 0, 0, 0);

        if (active)
        {
            background = IM_COL32(255, 255, 255, 40);
        }
        else if (hovered)
        {
            background = IM_COL32(255, 255, 255, 35);
        }

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            position,
            ImVec2(position.x + size.x, position.y + size.y),
            background);

        ImVec2 textSize = ImGui::CalcTextSize(text);
        drawList->AddText(
            ImVec2(
                position.x + (size.x - textSize.x) * 0.5f,
                position.y + (size.y - textSize.y) * 0.5f),
            IM_COL32(255, 255, 255, 255),
            text);

        ImGui::PopID();
        return clicked;
    }

    static bool drawPropertyInput(
        const char* label,
        float* value,
        float step,
        float min,
        float max,
        const char* format = "%.2f")
    {
        ImGui::PushID(label);
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 140.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        bool changed = ImGui::InputFloat("##val", value, step, step * 10.0f, format);
        *value = std::clamp(*value, min, max);
        ImGui::PopID();
        ImGui::Columns(1);
        return changed;
    }

    static bool drawPropertyInput(
        const char* label,
        uint32_t* value,
        float speed,
        uint32_t min,
        uint32_t max,
        const char* format = "%u")
    {
        ImGui::PushID(label);
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 140.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        bool changed = ImGui::DragScalar(
            "##val",
            ImGuiDataType_U32,
            value,
            speed,
            &min,
            &max,
            format);
        ImGui::PopID();
        ImGui::Columns(1);
        return changed;
    }

    LoadedAsset* MainWindow::findAsset(ResourceId id)
    {
        auto it = std::find_if(
            m_loadedAssets.begin(),
            m_loadedAssets.end(),
            [id](const LoadedAsset& asset)
            {
                return asset.id == id;
            });

        return it != m_loadedAssets.end() ? &*it : nullptr;
    }

    MainWindow::MainWindow(
        pal::WindowBase* window,
        engine::render::MainPassSettings initialMainPassSettings,
        bool isMaximized)
        : window(window),
          settings(initialMainPassSettings)
    {
        ImGuiIO& io = ImGui::GetIO();
        editorTitleFont = io.Fonts->AddFontFromFileTTF(
            "../resources/fonts/Inter-Medium.ttf",
            18.0f);
    }

    void MainWindow::drawUi()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize;

        // Вызов кроссплатформенного геттера PAL
        const bool isMaximized = window ? window->isMaximized() : false;
        if (isMaximized)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        }

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(0.0f, 0.0f));

        ImGui::Begin("MainWindow", nullptr, flags);
        drawTitleBar();
        drawClientArea();
        ImGui::End();

        ImGui::PopStyleVar(isMaximized ? 2 : 1);

        drawImportModals();
        drawExitConfirmationModal();
    }

    void MainWindow::pollFileDialogs()
    {
        if (sceneFileDialog && sceneFileDialog->ready())
        {
            auto result = sceneFileDialog->result();
            if (!result.empty() && openSceneCallback)
            {
                openSceneCallback(result.front());
            }
            sceneFileDialog.reset();
        }

        if (environmentFileDialog && environmentFileDialog->ready())
        {
            auto result = environmentFileDialog->result();
            if (!result.empty() && openEnvironmentCallback)
            {
                openEnvironmentCallback(result.front());
            }
            environmentFileDialog.reset();
        }

        if (importSceneFileDialog && importSceneFileDialog->ready())
        {
            auto result = importSceneFileDialog->result();
            if (!result.empty())
            {
                m_tempImportPath = result.front();
            }
            importSceneFileDialog.reset();
        }

        if (importEnvFileDialog && importEnvFileDialog->ready())
        {
            auto result = importEnvFileDialog->result();
            if (!result.empty())
            {
                m_tempImportPath = result.front();
            }
            importEnvFileDialog.reset();
        }

        if (saveFileDialog && saveFileDialog->ready())
        {
            auto result = saveFileDialog->result();
            if (!result.empty())
            {
                const std::filesystem::path savePath = result;
                if (pendingSaveType == AssetType::Scene && saveSceneCallback)
                {
                    saveSceneCallback(savePath);
                }
                else if (pendingSaveType == AssetType::Environment && saveEnvironmentCallback)
                {
                    saveEnvironmentCallback(savePath);
                }
            }
            saveFileDialog.reset();
        }
    }

    void MainWindow::drawFpsCounter()
    {
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::PushStyleColor(
            ImGuiCol_WindowBg,
            ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(
            ImGuiCol_Text,
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(viewport->Pos.x + 100.0f, viewport->Pos.y + 100.0f),
            ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("DEBUG_FPS", nullptr, flags))
        {
            ImGui::Text("FPS: %.0f", io.Framerate);
            ImGui::Text(
                "FrameTime: %.2f ms",
                io.Framerate > 0 ? 1000.0f / io.Framerate : 0.0f);
        }
        ImGui::End();

        ImGui::PopStyleColor(2);
    }

    void MainWindow::setResizeMode(bool enabled)
    {
        m_resizeMode = enabled;
    }

    bool MainWindow::isResizeMode() const
    {
        return m_resizeMode;
    }

    void MainWindow::drawTitleBar()
    {
        constexpr float titleBarHeight = 45.0f;
        constexpr float buttonWidth = 45.0f;
        constexpr float buttonsWidth = buttonWidth * 3.0f;
        constexpr float titleX = 25.0f;
        constexpr float fileX = 180.0f;

        const float windowWidth = ImGui::GetWindowWidth();

        // ---------------------------------------------------------------------
        // ОБНОВЛЕНИЕ ДЕКОРАЦИЙ И HIT-TEST ЗОН ЧЕРЕЗ PAL WINDOW
        // ---------------------------------------------------------------------
        if (window)
        {
            auto& titlebar = window->getDecorations().titlebar;
            titlebar.clearElements();
            titlebar.setBounds({ 0, 0, static_cast<int32_t>(windowWidth), static_cast<int32_t>(titleBarHeight) });

            // Кнопка меню File (не должна таскать окно)
            titlebar.addElement({
                .rect = { static_cast<int32_t>(fileX), 0, 60, static_cast<int32_t>(titleBarHeight) },
                .result = pal::WindowHitTestResult::Client
            });

            // Кнопка Minimize
            titlebar.addElement({
                .rect = { static_cast<int32_t>(windowWidth - buttonsWidth), 0, static_cast<int32_t>(buttonWidth), static_cast<int32_t>(titleBarHeight) },
                .result = pal::WindowHitTestResult::MinimizeButton
            });

            // Кнопка Maximize/Restore
            titlebar.addElement({
                .rect = { static_cast<int32_t>(windowWidth - buttonsWidth + buttonWidth), 0, static_cast<int32_t>(buttonWidth), static_cast<int32_t>(titleBarHeight) },
                .result = pal::WindowHitTestResult::MaximizeButton
            });

            // Кнопка Close
            titlebar.addElement({
                .rect = { static_cast<int32_t>(windowWidth - buttonWidth), 0, static_cast<int32_t>(buttonWidth), static_cast<int32_t>(titleBarHeight) },
                .result = pal::WindowHitTestResult::CloseButton
            });
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            ImVec4(0.10f, 0.10f, 0.10f, 1.0f));

        ImGui::BeginChild(
            "TitleBar",
            ImVec2(0.0f, titleBarHeight),
            false,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);

        const float textHeight = ImGui::GetTextLineHeight();
        ImGui::SetCursorPos(
            ImVec2(titleX, (titleBarHeight - textHeight) * 0.5f));
        ImGui::PushFont(editorTitleFont, 18.0f);
        ImGui::TextUnformatted("Shuttle Editor");
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(fileX, 0.0f));
        ImVec2 menuButtonPosition = ImGui::GetCursorScreenPos();

        if (drawTitleBarButton("File", "File", ImVec2(60.0f, titleBarHeight)))
        {
            ImGui::OpenPopup("FileMenu");
        }

        ImGui::SetNextWindowPos(
            ImVec2(
                menuButtonPosition.x,
                menuButtonPosition.y + titleBarHeight + 4.0f),
            ImGuiCond_Always);

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(12.0f, 8.0f));

        if (ImGui::BeginPopup("FileMenu"))
        {
            if (ImGui::MenuItem("Open Scene..."))
            {
                sceneFileDialog = std::make_unique<pfd::open_file>(
                    "Open Scene",
                    "",
                    std::vector<std::string>{
                        "Shuttle Scene", "*.sblb",
                        "All Files", "*"});
            }

            if (ImGui::MenuItem("Open Environment..."))
            {
                environmentFileDialog = std::make_unique<pfd::open_file>(
                    "Open Environment",
                    "",
                    std::vector<std::string>{
                        "Shuttle Environment", "*.sblb",
                        "All Files", "*"});
            }

            ImGui::Separator();

            bool canSave = false;
            LoadedAsset* assetToSave = nullptr;

            if (m_selectedSceneId != 0)
            {
                assetToSave = findAsset(m_selectedSceneId);
                canSave = assetToSave && assetToSave->isDirty;
            }
            else if (m_selectedEnvironmentId != 0)
            {
                assetToSave = findAsset(m_selectedEnvironmentId);
                canSave = assetToSave && assetToSave->isDirty;
            }

            if (ImGui::MenuItem("Save", nullptr, false, canSave) && assetToSave)
            {
                pendingSaveType = assetToSave->type;
                const bool isScene = assetToSave->type == AssetType::Scene;
                const std::string filterName = isScene ? "Shuttle Scene" : "Shuttle Environment";
                const std::string filterPattern = isScene ? "*.sblb" : "*.env";

                saveFileDialog = std::make_unique<pfd::save_file>(
                    "Save Asset As",
                    assetToSave->path.string(),
                    std::vector<std::string>{
                        filterName, filterPattern,
                        "All Files", "*"},
                    pfd::opt::none);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Import Scene..."))
            {
                m_tempImportPath.clear();
                m_pendingSceneOptions = assets::scene_compiler::SceneCompilerOptions{};
                m_openSceneModalRequested = true;
            }

            if (ImGui::MenuItem("Import Environment..."))
            {
                m_tempImportPath.clear();
                m_pendingIblSettings = engine::ibl::IblGenerationSettings{};
                m_openEnvModalRequested = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {
                tryExit();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(windowWidth - buttonsWidth, 0.0f));
        if (drawWindowButton("Minimize", WindowButtonType::Minimize, ImVec2(buttonWidth, titleBarHeight)))
        {
            if (window) window->minimize();
        }

        ImGui::SameLine(0.0f, 0.0f);

        const bool isMaximized = window ? window->isMaximized() : false;
        if (drawWindowButton(
                "Maximize",
                isMaximized ? WindowButtonType::Restore : WindowButtonType::Maximize,
                ImVec2(buttonWidth, titleBarHeight)))
        {
            if (window)
            {
                if (isMaximized)
                {
                    window->restore();
                }
                else
                {
                    window->maximize();
                }
            }
        }

        ImGui::SameLine(0.0f, 0.0f);
        if (drawWindowButton("Close", WindowButtonType::Close, ImVec2(buttonWidth, titleBarHeight)))
        {
            tryExit();
        }

        ImGui::EndChild();

        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(0.0f, titleBarHeight),
            ImVec2(windowWidth, titleBarHeight),
            IM_COL32(50, 50, 50, 255),
            2.0f);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void MainWindow::drawClientArea()
    {
        ImGui::BeginChild(
            "ClientArea",
            ImVec2(0.0f, 0.0f),
            false,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);

        drawRendererPanel();
        ImGui::SameLine(0.0f, 0.0f);

        ImVec2 separatorPosition = ImGui::GetCursorScreenPos();
        float height = ImGui::GetContentRegionAvail().y;

        ImGui::GetWindowDrawList()->AddLine(
            separatorPosition,
            ImVec2(separatorPosition.x, separatorPosition.y + height),
            IM_COL32(55, 55, 55, 255),
            2.0f);

        ImGui::SameLine(0.0f, 2.0f);
        drawViewportPanel();

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImGui::SetCursorPos(ImVec2(windowSize.x - 120.0f, 20.0f));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
        if (ImGui::BeginChild(
                "FPS_Overlay",
                ImVec2(100.0f, 30.0f),
                false,
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoNav))
        {
            ImGui::SetCursorPos(ImVec2(10.0f, 5.0f));
            ImGui::Text("FPS: %.0f", io.Framerate);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::EndChild();
    }

    void MainWindow::drawRendererPanel()
    {
        constexpr float panelWidth = 360.0f;

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImVec2(10.0f, 0.0f));
        ImGui::PushStyleVar(
            ImGuiStyleVar_SeparatorTextPadding,
            ImVec2(10.0f, 0.0f));

        ImGui::BeginChild(
            "RendererPanel",
            ImVec2(panelWidth, 0.0f),
            false,
            ImGuiWindowFlags_NoScrollbar);
        ImGui::Indent(10.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.20f, 0.23f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.28f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.35f, 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.22f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.24f, 0.27f, 0.33f, 1.0f));

        if (ImGui::BeginTabBar("SidebarTabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("  Assets  "))
            {
                ImGui::Spacing();

                auto drawAssetItem = [&](const LoadedAsset& asset, ResourceId& selectedIdRef)
                {
                    ImGui::PushID(static_cast<int>(asset.id));

                    const bool isSelected = selectedIdRef == asset.id;
                    const std::string label = (asset.isDirty ? "* " : "") + asset.name;
                    const float width = ImGui::GetContentRegionAvail().x;
                    constexpr float closeButtonWidth = 22.0f;

                    ImGui::Indent(5.0f);
                    if (ImGui::Selectable(
                            label.c_str(),
                            isSelected,
                            0,
                            ImVec2(width - closeButtonWidth * 3.0f / 2.0f, 0.0f)))
                    {
                        selectedIdRef = asset.id;
                        if (selectAssetCallback)
                        {
                            selectAssetCallback(asset.id);
                        }
                    }
                    ImGui::Unindent(5.0f);

                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Activate"))
                        {
                            selectedIdRef = asset.id;
                            if (selectAssetCallback)
                            {
                                selectAssetCallback(asset.id);
                            }
                        }

                        if (ImGui::MenuItem("Close Asset"))
                        {
                            m_assetsPendingDeletion.push_back(asset.id);
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::SameLine(width - closeButtonWidth / 2.0f);
                    if (ImGui::SmallButton(("X##" + std::to_string(asset.id)).c_str()))
                    {
                        m_assetsPendingDeletion.push_back(asset.id);
                    }

                    ImGui::PopID();
                };

                if (ImGui::CollapsingHeader(
                        "SCENES (3D Models)",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& asset : m_loadedAssets)
                    {
                        if (asset.type == AssetType::Scene)
                        {
                            drawAssetItem(asset, m_selectedSceneId);
                        }
                    }
                }

                if (ImGui::CollapsingHeader(
                        "ENVIRONMENTS (Skyboxes)",
                        ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto& asset : m_loadedAssets)
                    {
                        if (asset.type == AssetType::Environment)
                        {
                            drawAssetItem(asset, m_selectedEnvironmentId);
                        }
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  Camera  "))
            {
                drawCameraPanel();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  Renderer  "))
            {
                ImGui::Spacing();
                ImGui::TextDisabled("VIEWPORT & DEBUG");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Checkbox("Enable Debug View", &debugModeEnabled);

                if (!debugModeEnabled)
                {
                    viewportLayoutMode = ViewportLayoutMode::Single;
                    settings.debugModeOutput1 = engine::render::DebugMode::Final;
                }
                else
                {
                    ImGui::Spacing();
                    drawViewportLayoutSelector();
                    drawDebugOutputSelectors();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("POST-PROCESSING");
                ImGui::Separator();
                ImGui::Spacing();

                drawSliderProperty("Exposure", &settings.exposure, 0.001f, 2.0f, 0.001f, "%.3f");
                drawSliderProperty("Gamma", &settings.gamma, 0.1f, 4.0f, 0.01f, "%.2f");

                ImGui::Spacing();
                ImGui::TextDisabled("LIGHTING & IBL");
                ImGui::Separator();
                ImGui::Spacing();

                drawSliderProperty("Diffuse IBL", &settings.diffuseIblStrength, 0.0f, 10.0f, 0.01f, "%.2f");
                drawSliderProperty("Specular IBL", &settings.specularIblStrength, 0.0f, 10.0f, 0.01f, "%.2f");
                drawSliderProperty("Skybox Intensity", &settings.skyboxIntensity, 0.0f, 10.0f, 0.01f, "%.2f");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if (!m_assetsPendingDeletion.empty())
        {
            for (ResourceId id : m_assetsPendingDeletion)
            {
                if (closeAssetCallback)
                {
                    closeAssetCallback(id);
                }
                removeAsset(id);
            }
            m_assetsPendingDeletion.clear();
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
        ImGui::Unindent(10.0f);
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    void MainWindow::drawCameraPanel()
    {
        ImGui::Spacing();
        ImGui::TextDisabled("CAMERA CONTROLS");
        ImGui::Separator();
        ImGui::Spacing();

        drawSliderProperty("Move Speed", &camMoveSpeed, 0.1f, 100.0f, 0.1f, "%.1f m/s");
        drawSliderProperty("Rotation Speed", &camRotationSpeed, 0.1f, 10.0f, 0.1f, "%.1f rad/s");
        drawSliderProperty("Sensitivity", &camSensitivity, 0.01f, 2.0f, 0.005f, "%.3f");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("SHORTCUTS");
        ImGui::BulletText("WASD / QE — Перемещение");
        ImGui::BulletText("ПКМ + Мышь — Оглядеться");
        ImGui::BulletText("Shift — Ускорение (x2)");
    }

    void MainWindow::drawViewportLayoutSelector()
    {
        const char* layoutNames[] = {
            "Single",
            "Split Vertical",
            "Split Horizontal",
            "Quad"};
        int currentLayout = static_cast<int>(viewportLayoutMode);

        if (ImGui::Combo(
                "Viewport Layout",
                &currentLayout,
                layoutNames,
                IM_ARRAYSIZE(layoutNames)))
        {
            viewportLayoutMode = static_cast<ViewportLayoutMode>(currentLayout);
        }
    }

    static const char* debugModeNames[] = {
        "Final",
        "Albedo",
        "Normal",
        "Tangent",
        "Bitangent",
        "Metallic",
        "Roughness",
        "Ambient Occlusion",
        "Emissive",
        "UV",
        "Mesh Id",
        "Material Id",
        "Instance Id",
        "View Depth",
        "Linear Depth",
        "World Position",
        "World Normal"};

    static void drawDebugModeCombo(
        const char* label,
        engine::render::DebugMode& mode)
    {
        int value = static_cast<int>(mode);
        if (ImGui::Combo(
                label,
                &value,
                debugModeNames,
                IM_ARRAYSIZE(debugModeNames)))
        {
            mode = static_cast<engine::render::DebugMode>(value);
        }
    }

    void MainWindow::drawDebugOutputSelectors()
    {
        uint32_t visibleOutputCount = getVisibleOutputCount();
        drawDebugModeCombo("Output 1", settings.debugModeOutput1);
        if (visibleOutputCount >= 2)
        {
            drawDebugModeCombo("Output 2", settings.debugModeOutput2);
        }
        if (visibleOutputCount >= 3)
        {
            drawDebugModeCombo("Output 3", settings.debugModeOutput3);
        }
        if (visibleOutputCount >= 4)
        {
            drawDebugModeCombo("Output 4", settings.debugModeOutput4);
        }
    }

    void MainWindow::drawViewportImage(
        VkDescriptorSet descriptorSet,
        ImVec2 size)
    {
        if (descriptorSet != VK_NULL_HANDLE)
        {
            ImGui::Image(
                reinterpret_cast<ImTextureID>(descriptorSet),
                size);
        }
        else
        {
            ImGui::Dummy(size);
        }
    }

    void MainWindow::drawViewportPanel()
    {
        ImGui::BeginChild(
            "Viewport",
            ImVec2(0.0f, 0.0f),
            false,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        uint32_t width = std::max(1u, static_cast<uint32_t>(viewportSize.x));
        uint32_t height = std::max(1u, static_cast<uint32_t>(viewportSize.y));

        if (debugModeEnabled)
        {
            switch (viewportLayoutMode)
            {
                case ViewportLayoutMode::Single:
                    break;
                case ViewportLayoutMode::SplitVertical:
                    width /= 2;
                    break;
                case ViewportLayoutMode::SplitHorizontal:
                    height /= 2;
                    break;
                case ViewportLayoutMode::Quad:
                    width /= 2;
                    height /= 2;
                    break;
            }
        }

        if (!m_resizeMode)
        {
            if (width != viewportExtent.width || height != viewportExtent.height)
            {
                if (viewportExtent.width != 0 || viewportExtent.height != 0)
                {
                    needRecreateViewPortResources = true;
                }

                viewportExtent = vk::Extent2D{.width = width, .height = height};
            }
            else
            {
                needRecreateViewPortResources = false;
            }
        }

        if (debugModeEnabled)
        {
            switch (viewportLayoutMode)
            {
                case ViewportLayoutMode::Single:
                    drawViewportImage(viewportDebugDescriptorSets[0], viewportSize);
                    break;

                case ViewportLayoutMode::SplitVertical:
                {
                    const float halfWidth = viewportSize.x * 0.5f;
                    drawViewportImage(
                        viewportDebugDescriptorSets[0],
                        ImVec2(halfWidth, viewportSize.y));
                    ImGui::SameLine(0.0f, 0.0f);
                    drawViewportImage(
                        viewportDebugDescriptorSets[1],
                        ImVec2(viewportSize.x - halfWidth, viewportSize.y));
                    break;
                }

                case ViewportLayoutMode::SplitHorizontal:
                {
                    const float halfHeight = viewportSize.y * 0.5f;
                    drawViewportImage(
                        viewportDebugDescriptorSets[0],
                        ImVec2(viewportSize.x, halfHeight));
                    drawViewportImage(
                        viewportDebugDescriptorSets[1],
                        ImVec2(viewportSize.x, viewportSize.y - halfHeight));
                    break;
                }

                case ViewportLayoutMode::Quad:
                {
                    const ImVec2 cellSize{
                        viewportSize.x * 0.5f,
                        viewportSize.y * 0.5f};
                    drawViewportImage(viewportDebugDescriptorSets[0], cellSize);
                    ImGui::SameLine(0.0f, 0.0f);
                    drawViewportImage(viewportDebugDescriptorSets[1], cellSize);
                    drawViewportImage(viewportDebugDescriptorSets[2], cellSize);
                    ImGui::SameLine(0.0f, 0.0f);
                    drawViewportImage(viewportDebugDescriptorSets[3], cellSize);
                    break;
                }
            }
        }
        else
        {
            drawViewportImage(outDescriptorSet, viewportSize);
        }

        ImGui::EndChild();
    }

    void MainWindow::drawImportModals()
    {
        if (m_openSceneModalRequested)
        {
            ImGui::OpenPopup("Import Scene Settings");
            m_openSceneModalRequested = false;
        }

        if (m_openEnvModalRequested)
        {
            ImGui::OpenPopup("Import Environment Settings");
            m_openEnvModalRequested = false;
        }

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.22f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.24f, 0.27f, 0.33f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(
                "Import Scene Settings",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Source File:");
            ImGui::SameLine();

            char pathBuffer[1024];
            std::strncpy(
                pathBuffer,
                m_tempImportPath.string().c_str(),
                sizeof(pathBuffer) - 1);
            pathBuffer[sizeof(pathBuffer) - 1] = '\0';

            float buttonWidth =
                ImGui::CalcTextSize("...").x +
                ImGui::GetStyle().FramePadding.x * 2.0f;
            float inputWidth =
                ImGui::GetContentRegionAvail().x -
                buttonWidth -
                ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetNextItemWidth(inputWidth);
            if (ImGui::InputText("##FilePath", pathBuffer, sizeof(pathBuffer)))
            {
                m_tempImportPath = pathBuffer;
            }

            ImGui::SameLine();
            if (ImGui::Button("...", ImVec2(buttonWidth, 0)))
            {
                importSceneFileDialog = std::make_unique<pfd::open_file>(
                    "Select Scene File",
                    m_tempImportPath.parent_path().string(),
                    std::vector<std::string>{
                        "FBX/glTF Files", "*.fbx *.gltf *.glb",
                        "All Files", "*"});
            }

            ImGui::Separator();

            if (ImGui::CollapsingHeader(
                    "Texture Resolver Options",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Scan Source Directory", &m_pendingSceneOptions.textureResolverOptions.scanSourceDirectory);
                ImGui::Checkbox("Resolve Albedo", &m_pendingSceneOptions.textureResolverOptions.resolveAlbedo);
                ImGui::Checkbox("Resolve Normal", &m_pendingSceneOptions.textureResolverOptions.resolveNormal);
                ImGui::Checkbox("Resolve ORM", &m_pendingSceneOptions.textureResolverOptions.resolveOrm);
                ImGui::Checkbox("Resolve Occlusion", &m_pendingSceneOptions.textureResolverOptions.resolveOcclusion);
                ImGui::Checkbox("Resolve Roughness", &m_pendingSceneOptions.textureResolverOptions.resolveRoughness);
                ImGui::Checkbox("Resolve Metallic", &m_pendingSceneOptions.textureResolverOptions.resolveMetallic);
                ImGui::Checkbox("Resolve Emissive From Catalog", &m_pendingSceneOptions.textureResolverOptions.resolveEmissiveFromCatalog);
            }

            if (ImGui::CollapsingHeader(
                    "Texture Compiler Options",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Compile Textures", &m_pendingSceneOptions.textureCompilerOptions.compileTextures);
                ImGui::Checkbox("Generate ORM Textures", &m_pendingSceneOptions.textureCompilerOptions.generateOrmTextures);
                ImGui::Checkbox("Generate Mips", &m_pendingSceneOptions.textureCompilerOptions.generateMips);
                ImGui::Checkbox("Flip Y (for textures)", &m_pendingSceneOptions.textureCompilerOptions.flipY);
                ImGui::Checkbox("Roughness is Gloss (invert roughness)", &m_pendingSceneOptions.textureCompilerOptions.roughnessIsGloss);
            }

            if (ImGui::CollapsingHeader(
                    "Geometry Options",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Generate LODs", &m_pendingSceneOptions.geometryOptions.generateLods);
                if (m_pendingSceneOptions.geometryOptions.generateLods)
                {
                    drawPropertyInput("Max LOD Count", &m_pendingSceneOptions.geometryOptions.maxLodCount, 1.0f, 1, 8);
                    drawPropertyInput("LOD1 Ratio", &m_pendingSceneOptions.geometryOptions.lod1Ratio, 0.01f, 0.0f, 1.0f);
                    drawPropertyInput("LOD2 Ratio", &m_pendingSceneOptions.geometryOptions.lod2Ratio, 0.01f, 0.0f, 1.0f);
                    drawPropertyInput("LOD3 Ratio", &m_pendingSceneOptions.geometryOptions.lod3Ratio, 0.01f, 0.0f, 1.0f);
                    drawPropertyInput("Simplify Error", &m_pendingSceneOptions.geometryOptions.simplifyTargetError, 0.001f, 0.0f, 1.0f, "%.4f");
                }
                ImGui::Checkbox("Optimize Vertex Cache", &m_pendingSceneOptions.geometryOptions.optimizeVertexCache);
                ImGui::Checkbox("Optimize Vertex Fetch", &m_pendingSceneOptions.geometryOptions.optimizeVertexFetch);
                ImGui::Checkbox("Optimize Overdraw", &m_pendingSceneOptions.geometryOptions.optimizeOverdraw);
            }

            ImGui::Separator();
            if (ImGui::Button("Import", ImVec2(120, 0)))
            {
                if (importSceneCallback)
                {
                    importSceneCallback(m_tempImportPath, m_pendingSceneOptions);
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(
                "Import Environment Settings",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Source File:");
            ImGui::SameLine();

            char pathBuffer[1024];
            std::strncpy(
                pathBuffer,
                m_tempImportPath.string().c_str(),
                sizeof(pathBuffer) - 1);
            pathBuffer[sizeof(pathBuffer) - 1] = '\0';

            float buttonWidth =
                ImGui::CalcTextSize("...").x +
                ImGui::GetStyle().FramePadding.x * 2.0f;
            float inputWidth =
                ImGui::GetContentRegionAvail().x -
                buttonWidth -
                ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetNextItemWidth(inputWidth);
            if (ImGui::InputText("##FilePathEnv", pathBuffer, sizeof(pathBuffer)))
            {
                m_tempImportPath = pathBuffer;
            }

            ImGui::SameLine();
            if (ImGui::Button("...", ImVec2(buttonWidth, 0)))
            {
                importEnvFileDialog = std::make_unique<pfd::open_file>(
                    "Select Environment File",
                    m_tempImportPath.parent_path().string(),
                    std::vector<std::string>{
                        "HDR Files", "*.hdr",
                        "All Files", "*"});
            }

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::CollapsingHeader(
                    "Environment Options",
                    ImGuiTreeNodeFlags_DefaultOpen))
            {
                drawPropertyInput("Skybox Size", &m_pendingIblSettings.skyboxSize, 1.0f, 32, 2048);
                drawPropertyInput("Irradiance Size", &m_pendingIblSettings.irradianceSize, 1.0f, 16, 128);
                drawPropertyInput("Radiance Size", &m_pendingIblSettings.radianceSize, 1.0f, 16, 128);
                drawPropertyInput("Irradiance Samples", &m_pendingIblSettings.irradianceSamples, 1.0f, 16, 128);
                drawPropertyInput("Radiance Samples", &m_pendingIblSettings.radianceSamples, 1.0f, 16, 128);
            }

            ImGui::Separator();
            if (ImGui::Button("Import", ImVec2(120, 0)))
            {
                if (importEnvironmentCallback)
                {
                    importEnvironmentCallback(m_tempImportPath, m_pendingIblSettings);
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    void MainWindow::drawExitConfirmationModal() const
    {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(
                "Unsaved Changes",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("You have unsaved changes. Do you want to save them before exiting?");
            ImGui::Separator();

            if (ImGui::Button("Save & Exit", ImVec2(120, 0)))
            {
                for (auto& asset : m_loadedAssets)
                {
                    if (!asset.isDirty)
                    {
                        continue;
                    }

                    if (asset.type == AssetType::Scene && saveSceneCallback)
                    {
                        saveSceneCallback(asset.path);
                    }
                    else if (asset.type == AssetType::Environment && saveEnvironmentCallback)
                    {
                        saveEnvironmentCallback(asset.path);
                    }
                }

                if (window) window->close();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Exit Without Saving", ImVec2(150, 0)))
            {
                if (window) window->close();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void MainWindow::tryExit()
    {
        const bool hasDirty = std::any_of(
            m_loadedAssets.begin(),
            m_loadedAssets.end(),
            [](const LoadedAsset& asset)
            {
                return asset.isDirty;
            });

        if (hasDirty)
        {
            ImGui::OpenPopup("Unsaved Changes");
        }
        else
        {
            if (window) window->close();
        }
    }

    void MainWindow::addAsset(LoadedAsset&& asset)
    {
        const ResourceId id = asset.id;
        const AssetType type = asset.type;
        m_loadedAssets.push_back(std::move(asset));

        if (type == AssetType::Scene)
        {
            m_selectedSceneId = id;
        }
        else
        {
            m_selectedEnvironmentId = id;
        }
    }

    void MainWindow::removeAsset(ResourceId id)
    {
        auto it = std::find_if(
            m_loadedAssets.begin(),
            m_loadedAssets.end(),
            [id](const LoadedAsset& asset)
            {
                return asset.id == id;
            });

        if (it == m_loadedAssets.end())
        {
            return;
        }

        if (m_selectedSceneId == id)
        {
            m_selectedSceneId = 0;
        }
        if (m_selectedEnvironmentId == id)
        {
            m_selectedEnvironmentId = 0;
        }

        m_loadedAssets.erase(it);
    }

    void MainWindow::markAssetDirty(ResourceId id, bool dirty)
    {
        auto it = std::find_if(
            m_loadedAssets.begin(),
            m_loadedAssets.end(),
            [id](const LoadedAsset& asset)
            {
                return asset.id == id;
            });

        if (it != m_loadedAssets.end())
        {
            it->isDirty = dirty;
        }
    }

    void MainWindow::markAssetSaved(
        ResourceId id,
        const std::string& newName,
        const std::filesystem::path& newPath)
    {
        auto it = std::find_if(
            m_loadedAssets.begin(),
            m_loadedAssets.end(),
            [id](const LoadedAsset& asset)
            {
                return asset.id == id;
            });

        if (it != m_loadedAssets.end())
        {
            it->isDirty = false;
            it->name = newName;
            it->path = newPath;
        }
    }

    VkDescriptorSet MainWindow::createViewportDescriptorSet(vk::ImageView imageView)
    {
        return ImGui_ImplVulkan_AddTexture(
            imageView,
            static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
    }

    UniqueViewportDescriptorSet MainWindow::createViewportDescriptorSetUnique(
        vk::ImageView imageView)
    {
        return UniqueViewportDescriptorSet{
            createViewportDescriptorSet(imageView)};
    }

    bool MainWindow::hasViewport() const
    {
        if (!debugModeEnabled)
        {
            return outDescriptorSet != VK_NULL_HANDLE;
        }

        const uint32_t count = getVisibleOutputCount();
        for (uint32_t i = 0; i < count; ++i)
        {
            if (viewportDebugDescriptorSets[i] == VK_NULL_HANDLE)
            {
                return false;
            }
        }
        return true;
    }

    uint32_t MainWindow::getVisibleOutputCount() const
    {
        if (!debugModeEnabled)
        {
            return 1;
        }

        switch (viewportLayoutMode)
        {
            case ViewportLayoutMode::Single:
                return 1;
            case ViewportLayoutMode::SplitVertical:
            case ViewportLayoutMode::SplitHorizontal:
                return 2;
            case ViewportLayoutMode::Quad:
                return 4;
        }

        return 1;
    }

    bool MainWindow::needViewPortResourcesRecreate() const
    {
        return needRecreateViewPortResources;
    }

    vk::Extent2D MainWindow::getViewportExtent() const
    {
        return viewportExtent;
    }

    void MainWindow::setFinalViewportImage(vk::DescriptorSet finalSet)
    {
        outDescriptorSet = finalSet;
    }

    void MainWindow::setDebugViewportImages(
        std::array<VkDescriptorSet, 4> debugSets)
    {
        viewportDebugDescriptorSets = debugSets;
    }

    void ResourceBin::release(ResourceId id)
    {
        for (size_t i = 0; i < m_images.size();)
        {
            if (m_images[i].id == id)
            {
                m_images[i] = std::move(m_images.back());
                m_images.pop_back();
            }
            else
            {
                ++i;
            }
        }

        for (size_t i = 0; i < m_descriptorSets.size();)
        {
            if (m_descriptorSets[i].id == id)
            {
                m_descriptorSets[i] = std::move(m_descriptorSets.back());
                m_descriptorSets.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

} // namespace shuttle::editor::core