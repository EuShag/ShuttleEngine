#ifndef HELLOTRIANGLE_UIRENDER_HPP
#define HELLOTRIANGLE_UIRENDER_HPP

#include "IncludeVulkan.hpp"
#include "PAL/Platform.hpp"
#include "PAL/Common/Window/WindowBase.hpp"

namespace shuttle
{
    struct IUiPainter {
        virtual void drawUi() = 0;
        virtual ~IUiPainter() = default;
    };

    struct UiPassInfo {
        vk::ImageView colorAttachment;
        vk::Extent2D extent;
    };

    class ImGuiContextM {
    public:
        static vk::ResultValue<ImGuiContextM> create(
            pal::WindowBase& window,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            uint32_t queueFamilyIndex,
            vk::Queue queue,
            uint32_t imageCount);

        void drawUi(IUiPainter &painter) const;

        void writeRenderCommands(vk::CommandBuffer cmd, UiPassInfo const& info) const;

        ImGuiContextM() = default;

        ImGuiContextM(const ImGuiContextM&) = delete;
        ImGuiContextM& operator=(const ImGuiContextM&) = delete;

        ImGuiContextM(ImGuiContextM&& other) noexcept
            : uiDescriptorPool(std::move(other.uiDescriptorPool))
            , m_platform(other.m_platform)
            , m_imguiContext(other.m_imguiContext)
        {
            other.m_platform = nullptr;
            other.m_imguiContext = nullptr;
        }

        ImGuiContextM& operator=(ImGuiContextM&& other) noexcept
        {
            if (this != &other)
            {
                uiDescriptorPool = std::move(other.uiDescriptorPool);
                m_platform = other.m_platform;
                m_imguiContext = other.m_imguiContext;

                other.m_platform = nullptr;
                other.m_imguiContext = nullptr;
            }
            return *this;
        }

        ~ImGuiContextM();

    private:
        vk::UniqueDescriptorPool uiDescriptorPool{};
        void* m_imguiContext = nullptr;
        pal::Platform* m_platform = nullptr; // Сохраняем ссылку на платформу для зачистки бэкенда
    };
} // namespace shuttle

#endif // HELLOTRIANGLE_UIRENDER_HPP
