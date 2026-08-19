//
// Created by Shagu on 14.06.2026.
//

#ifndef HELLOTRIANGLE_UIRENDER_HPP
#define HELLOTRIANGLE_UIRENDER_HPP
#include <imgui.h>

#include "IncludeVulkan.hpp"
#include "Sdl/SdlLibrary/SdlLibrary.hpp"
#include "Sdl/SdlWindow/SdlWindow.hpp"

class SdlWindow;

namespace shuttle
{

struct UiTargets
{
};

class UiRender
{
  public:
    static vk::ResultValue<UiRender> create(SdlWindow& window, vk::Instance instance, vk::PhysicalDevice physicalDevice,
                                            vk::Device device, uint32_t queueFamilyIndex, vk::Queue queue,
                                            uint32_t imageCount);

    UiRender() = default;

    UiRender(const UiRender&) = delete;
    UiRender(UiRender&& other) noexcept {
        uiDescriptorPool = std::move(other.uiDescriptorPool);
        device = other.device;
        other.device = VK_NULL_HANDLE;
    }
    UiRender& operator=(const UiRender&) = delete;
    UiRender& operator=(UiRender&& other) noexcept {
        uiDescriptorPool = std::move(other.uiDescriptorPool);
        device = other.device;
        other.device = VK_NULL_HANDLE;
        return *this;
    }

    static void bindInputEventHandler(SdlLibrary& library);

    bool operator==(UiRender const& ui_render) const;

    bool operator!=(UiRender const &ui_render_result) const;

    ~UiRender();

  private:
    vk::UniqueDescriptorPool uiDescriptorPool{};
    vk::Device device{};
};
} // namespace shuttle

#endif // HELLOTRIANGLE_UIRENDER_HPP
