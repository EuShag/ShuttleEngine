//
// Created by Shagu on 14.06.2026.
//

#ifndef HELLOTRIANGLE_UIRENDER_HPP
#define HELLOTRIANGLE_UIRENDER_HPP
#include <imgui.h>

#include "IncludeVulkan.hpp"
#include "Sdl/SdlLibrary/SdlLibrary.hpp"
#include "Sdl/SdlWindow/SdlWindow.hpp"

namespace shuttle_engine {

    class IuiPainter {
    public:
        virtual void drawUi() = 0;
        virtual ~IuiPainter() = default;
    };

    class HelloWorldPainter : public IuiPainter {
    public:
        void drawUi() override {
            ImGui::Begin("Hello World");
            ImGui::Text("Hello World");
            ImGui::End();
        }

    };

    class DemoWindowPainter : public IuiPainter {
    public:
        void drawUi() override {
            ImGui::ShowDemoWindow();
        }
    };

    class FPSCounterPainter : public shuttle_engine::IuiPainter {
    public:
        void drawUi() override {
            // Устанавливаем прозрачное окно в углу или обычное окошко
            ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            float fps = ImGui::GetIO().Framerate;
            float ms = 1000.0f / fps;

            // Выводим текст с цветом (зеленый если > 60 FPS, желтый если ниже)
            ImGui::Text("FPS: ");
            ImGui::SameLine();
            ImGui::TextColored(fps > 60.0f ? ImVec4(0,1,0,1) : ImVec4(1,1,0,1), "%.1f", fps);

            ImGui::Text("Frame Time: %.3f ms", ms);

            // Добавим маленький график для наглядности (опционально)
            // Мы используем static, чтобы хранить историю между вызовами const метода
            static float values[90] = { 0 };
            static int values_offset = 0;
            static double refresh_time = 0.0;

            if (refresh_time == 0.0) refresh_time = ImGui::GetTime();

            // Обновляем график каждые 0.1 сек, чтобы он не летел слишком быстро
            while (refresh_time < ImGui::GetTime()) {
                values[values_offset] = ms;
                values_offset = (values_offset + 1) % 90;
                refresh_time += 1.0 / 30.0; // 30 обновлений в секунду
            }

            ImGui::PlotLines("Latency", values, 90, values_offset, nullptr, 0.0f, 33.0f, ImVec2(0, 50));

            ImGui::End();
        }
    };

    struct UiTargets {};

    class UiRender {
    public:
        static vk::ResultValue<UiRender> create(
            SdlWindow& window,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            uint32_t queueFamilyIndex,
            vk::Queue queue,
            uint32_t imageCount,
            vk::RenderPass renderPass
        );

        void bindInputEventHandler(SdlLibrary& library);

        void drawUi(IuiPainter && painter);

        void recordDrawCommands(vk::CommandBuffer cmdBuffer) const;

        void destroy(vk::Device device);

    private:
        vk::UniqueDescriptorPool uiDescriptorPool;
    };
} // shuttle_engine

#endif //HELLOTRIANGLE_UIRENDER_HPP
