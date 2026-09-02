#include "UiRender.hpp"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#if defined(_WIN32)
    #include <windows.h>
    #include <vulkan/vulkan_win32.h>

    static int ImGui_Platform_CreateVkSurface(
        ImGuiViewport* viewport,
        ImU64 vk_instance,
        const void* vk_allocator,
        ImU64* out_vk_surface)
    {
        VkInstance instance = reinterpret_cast<VkInstance>(vk_instance);

        // Динамически запрашиваем указатель на vkCreateWin32SurfaceKHR через Dispatcher
        auto pfnCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
            VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR")
        );

        if (!pfnCreateWin32SurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = static_cast<HWND>(viewport->PlatformHandleRaw ? viewport->PlatformHandleRaw : viewport->PlatformHandle);
        createInfo.hinstance = GetModuleHandle(nullptr);

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult err = pfnCreateWin32SurfaceKHR(
            instance,
            &createInfo,
            static_cast<const VkAllocationCallbacks*>(vk_allocator),
            &surface);

        if (err != VK_SUCCESS)
        {
            return static_cast<int>(err);
        }

        *out_vk_surface = reinterpret_cast<ImU64>(surface);
        return 0;
    }
#endif


namespace shuttle
{
    thread_local VkResult UiRenderCreateResult = VK_SUCCESS;

    vk::ResultValue<ImGuiContextM> ImGuiContextM::create(
        pal::WindowBase& window,
        vk::Instance instance,
        vk::PhysicalDevice physicalDevice,
        vk::Device device,
        uint32_t queueFamilyIndex,
        vk::Queue queue,
        uint32_t imageCount)
    {
        ImGuiContextM result;
        result.m_platform = &window.platform(); // Сохраняем указатель на платформу

        std::array poolSizes = {
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageImage, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformTexelBuffer, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageTexelBuffer, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBufferDynamic, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBufferDynamic, .descriptorCount = 100},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eInputAttachment, .descriptorCount = 100}
        };

        auto [createDescriptorPoolResult, uniqueDescriptorPool] =
            device.createDescriptorPoolUnique({
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = 100,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data()
            });

        if (createDescriptorPoolResult != vk::Result::eSuccess) return {createDescriptorPoolResult, {}};

        result.uiDescriptorPool = std::move(uniqueDescriptorPool);

        IMGUI_CHECKVERSION();
        ::ImGuiContext* context = ImGui::CreateContext();
        ImGui::SetCurrentContext(context);
        result.m_imguiContext = context;
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        // ---------------------------------------------------------------------
        // ИНИЦИАЛИЗАЦИЯ ПЛАТФОРМЕННОГО БЭКЕНДА (Win32 или SDL2)
        // ---------------------------------------------------------------------
        window.platform().initGuiBackend(window.getHandle());

#if defined(_WIN32)
        // Регистрируем обработчик для создания поверхностей Vulkan в ImGui (для мульти-вьюпортов / выплывающих окон)
        ImGui::GetPlatformIO().Platform_CreateVkSurface = ImGui_Platform_CreateVkSurface;
#endif

        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;

        ImGui_ImplVulkan_InitInfo initInfo{
            .ApiVersion = vk::makeApiVersion(0, 1, 4, 0),
            .Instance = instance,
            .PhysicalDevice = physicalDevice,
            .Device = device,
            .QueueFamily = queueFamilyIndex,
            .Queue = queue,
            .DescriptorPool = *result.uiDescriptorPool,
            .MinImageCount = imageCount,
            .ImageCount = imageCount,
            .PipelineInfoMain = {
                .MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &colorFormat,
                    .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
                }
            },
            .UseDynamicRendering = true,
            .Allocator = nullptr,
            .CheckVkResultFn = [](VkResult const res) { UiRenderCreateResult = res; },
            .MinAllocationSize = 2048 * 2048
        };

        ImGui_ImplVulkan_LoadFunctions(
            VK_API_VERSION_1_4,
            [](const char* function_name, void* user_data)
            {
                return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                    static_cast<VkInstance>(*static_cast<vk::Instance*>(user_data)), function_name);
            },
            &instance
        );

        ImGui_ImplVulkan_Init(&initInfo);
        window.setImGuiContext(context);

        return {static_cast<vk::Result>(UiRenderCreateResult), std::move(result)};
    }

    void ImGuiContextM::drawUi(IUiPainter &painter) const {
        auto oldContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imguiContext));
        ImGui_ImplVulkan_NewFrame();
        m_platform->newGuiFrame();
        ImGui::NewFrame();
        painter.drawUi();
        ImGui::Render();
        ImGui::SetCurrentContext(oldContext);
    }

    void ImGuiContextM::writeRenderCommands(vk::CommandBuffer cmd, UiPassInfo const &info) const {
        auto oldContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imguiContext));

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
        ImGui::SetCurrentContext(oldContext);
    }


    ImGuiContextM::~ImGuiContextM() {
        if (m_imguiContext){
            auto oldContext = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_imguiContext));
            // Завершаем работу ImGui Vulkan backend
            ImGui_ImplVulkan_Shutdown();

            // Завершаем работу платформа-специфичного GUI backend (Win32 / SDL2) через PAL
            if (m_platform)
            {
                m_platform->shutdownGuiBackend();
            }
            ImGui::DestroyContext(static_cast<ImGuiContext*>(m_imguiContext));
            ImGui::SetCurrentContext(oldContext);
        }
    }
}