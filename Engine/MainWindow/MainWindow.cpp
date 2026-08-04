//
// Created by Shagu on 04.08.2026.
//

#include "MainWindow.hpp"

#include "imgui.h"

namespace shuttle::editor::core {
    void MainWindow::drawUi()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking;

        ImGui::Begin("MainWindow", nullptr, flags);

        drawMenuBar();

        ImGuiID dockSpaceId =
            ImGui::GetID("MainDockSpace");

        ImGui::DockSpace(
            dockSpaceId,
            ImVec2(0, 0)
        );

        ImGui::End();
    }
}
