//
// Created by Shagu on 04.08.2026.
//

#include "uiPass.hpp"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "PAL/Platform.hpp"

namespace shuttle::engine::render {

    void UiPass::drawUi(IuiPainter& painter, pal::Platform const& platform)
    {
        ImGui_ImplVulkan_NewFrame();
        platform.newGuiFrame();
        ImGui::NewFrame();
        painter.drawUi();
        ImGui::Render();
    }

    void UiPass::writeRenderCommands(
        vk::CommandBuffer cmd,
        UiPassInfo const& info)
    {
        vk::RenderingAttachmentInfo colorAttachment {
            .imageView = info.colorAttachment,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                .color = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}
            },
        };

        cmd.beginRendering({
            .renderArea ={ .offset = {.x = 0, .y = 0}, .extent = info.extent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        });

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        cmd.endRendering();
    }
}
