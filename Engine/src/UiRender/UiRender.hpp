#ifndef HELLOTRIANGLE_UIRENDER_HPP
#define HELLOTRIANGLE_UIRENDER_HPP

#include <imgui.h>
#include "IncludeVulkan.hpp"
#include "PAL/Platform.hpp"
#include "PAL/Common/Window/WindowBase.hpp"

namespace shuttle
{
    struct UiTargets {};

    class UiRender
    {
    public:
        static vk::ResultValue<UiRender> create(
            pal::WindowBase& window,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            uint32_t queueFamilyIndex,
            vk::Queue queue,
            uint32_t imageCount);

        UiRender() = default;

        UiRender(const UiRender&) = delete;
        UiRender& operator=(const UiRender&) = delete;

        UiRender(UiRender&& other) noexcept
            : uiDescriptorPool(std::move(other.uiDescriptorPool))
            , device(other.device)
            , m_platform(other.m_platform)
        {
            other.device = VK_NULL_HANDLE;
            other.m_platform = nullptr;
        }

        UiRender& operator=(UiRender&& other) noexcept
        {
            if (this != &other)
            {
                uiDescriptorPool = std::move(other.uiDescriptorPool);
                device = other.device;
                m_platform = other.m_platform;

                other.device = VK_NULL_HANDLE;
                other.m_platform = nullptr;
            }
            return *this;
        }

        bool operator==(UiRender const& ui_render) const;
        bool operator!=(UiRender const& ui_render_result) const;

        ~UiRender();

    private:
        vk::UniqueDescriptorPool uiDescriptorPool{};
        vk::Device device{};
        pal::Platform* m_platform = nullptr; // Сохраняем ссылку на платформу для зачистки бэкенда
    };
} // namespace shuttle

#endif // HELLOTRIANGLE_UIRENDER_HPP
