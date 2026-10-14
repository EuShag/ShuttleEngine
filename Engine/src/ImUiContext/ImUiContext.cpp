#include "ImUiContext.hpp"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "Engine/ViewportResources.hpp"

namespace shuttle
{
    namespace {
        thread_local VkResult UiRenderCreateResult = VK_SUCCESS;

        class [[nodiscard]] ContextGuard {
        public:
            // Конструктор: запоминает текущий контекст из TLS и выставляет целевой
            explicit ContextGuard(ImGuiContext* targetContext) noexcept
                : m_oldContext(ImGui::GetCurrentContext())
            {
                ImGui::SetCurrentContext(targetContext);
            }

            // Деструктор: автоматически возвращает старый контекст на место
            ~ContextGuard() noexcept {
                ImGui::SetCurrentContext(m_oldContext);
            }

            // Запрещаем копирование и перемещение, так как страж привязан к scope (области видимости)
            ContextGuard(const ContextGuard&) = delete;
            ContextGuard& operator=(const ContextGuard&) = delete;
            ContextGuard(ContextGuard&&) = delete;
            ContextGuard& operator=(ContextGuard&&) = delete;

        private:
            ImGuiContext* m_oldContext;
        };
    }

    void ViewportTexturesDeleter::operator()(ViewportTexturesHandle handle) const {
        m_imguiContext->destroyViewportTextures(handle);
    }

    vk::ResultValue<ImUiContext> ImUiContext::create(
        pal::WindowBase& window,
        vk::Instance instance,
        vk::PhysicalDevice physicalDevice,
        vk::Device device,
        vk::detail::DispatchLoaderDynamic const& dispatcher,
        uint32_t queueFamilyIndex,
        vk::Queue queue,
        uint32_t imageCount)
    {
        ImUiContext result;
        result.m_platform = &window.platform(); // Сохраняем указатель на платформу
        UiRenderCreateResult = VK_SUCCESS;

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
        ContextGuard contextGuard(context);
        result.m_imguiContext = context;
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        // ---------------------------------------------------------------------
        // ИНИЦИАЛИЗАЦИЯ ПЛАТФОРМЕННОГО БЭКЕНДА (Win32 или SDL2)
        // ---------------------------------------------------------------------
        window.platform().initGuiBackend(window.getHandle());

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

        struct LoadFunctionsData {
            vk::detail::DispatchLoaderDynamic const* m_dispatcher;
            vk::Instance m_instance;
        };

        LoadFunctionsData loadFunctionsData{
            .m_dispatcher = &dispatcher,
            .m_instance = instance
        };

        ImGui_ImplVulkan_LoadFunctions(
            VK_API_VERSION_1_4,
            [](const char* function_name, void* user_data)
            {
                auto const * loadFunctionsData = static_cast<LoadFunctionsData*>(user_data);
                auto const& dispatcher = *loadFunctionsData->m_dispatcher;
                auto instance = loadFunctionsData->m_instance;

                return dispatcher.vkGetInstanceProcAddr(instance, function_name);
            },
            &loadFunctionsData
        );

        ImGui_ImplVulkan_Init(&initInfo);
        window.setImGuiContext(context);
        result.m_window = &window;

        return {static_cast<vk::Result>(UiRenderCreateResult), std::move(result)};
    }

    void ImUiContext::drawUi(IUiPainter &painter, uint32_t frameIndex) const {
        ContextGuard ctx(static_cast<ImGuiContext *>(m_imguiContext));
        ImGui_ImplVulkan_NewFrame();
        m_platform->newGuiFrame();
        ImGui::NewFrame();
        painter.drawUi(frameIndex);
        ImGui::Render();
    }

    void ImUiContext::writeRenderCommands(vk::CommandBuffer cmd) const {
        ContextGuard ctx(static_cast<ImGuiContext *>(m_imguiContext));
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }

    vk::ResultValue<vk::DescriptorSet> ImUiContext::createImGuiTextureDescriptorSet(vk::ImageView imageView) const {
        ContextGuard ctx(static_cast<ImGuiContext *>(m_imguiContext));
        VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
            imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return {vk::Result::eSuccess, descriptorSet};
        m_textureCount.fetch_add(1U, std::memory_order_relaxed);
    }

    vk::ResultValue<ImGuiUniqueDescriptorSet> ImUiContext::createImGuiTextureDescriptorSetUnique(vk::ImageView imageView) const {
        auto [result, descriptorSet] = createImGuiTextureDescriptorSet(imageView);
        return {result, vk::UniqueHandle<vk::DescriptorSet, ImUiContext>{
            descriptorSet, vk::UniqueHandleTraits<vk::DescriptorSet, ImUiContext>::deleter{*this}}};
    }

    void ImUiContext::destroyImGuiTextureDescriptorSet(vk::DescriptorSet descriptorSet) const {
        ContextGuard ctx(static_cast<ImGuiContext *>(m_imguiContext));
        ImGui_ImplVulkan_RemoveTexture(descriptorSet);
        m_textureCount.fetch_sub(1U, std::memory_order_relaxed);
    }

    vk::ResultValue<ViewportTexturesHandle> ImUiContext::createViewportTextures(
        engine::ViewportResources const &resources) {

        ViewportTextures textures;

        for (int i = 0; i < resources.m_colorAttachmentOutputs.size(); i++) {

            auto& colorImageView = *resources.m_colorAttachmentOutputs[i].view;
            auto& debug1ImageView = *resources.m_debugOutputs1[i].view;
            auto& debug2ImageView = *resources.m_debugOutputs2[i].view;
            auto& debug3ImageView = *resources.m_debugOutputs3[i].view;
            auto& debug4ImageView = *resources.m_debugOutputs4[i].view;

            auto [createFinalDescriptorSetResult, finalDescriptorSet] = createImGuiTextureDescriptorSetUnique(
                colorImageView
            );
            if (createFinalDescriptorSetResult != vk::Result::eSuccess) {
                return {createFinalDescriptorSetResult, {}};
            }
            textures.m_colorAttachmentSets.push_back(std::move(finalDescriptorSet));
            auto [createDebug1DescriptorSetResult, debug1DescriptorSet] = createImGuiTextureDescriptorSetUnique(
                debug1ImageView
            );
            if (createDebug1DescriptorSetResult != vk::Result::eSuccess) {
                return {createDebug1DescriptorSetResult, {}};
            }
            textures.m_debugAttachmentSets1.push_back(std::move(debug1DescriptorSet));
            auto [createDebug2DescriptorSetResult, debug2DescriptorSet] = createImGuiTextureDescriptorSetUnique(
                debug2ImageView
            );
            if (createDebug2DescriptorSetResult != vk::Result::eSuccess) {
                return {createDebug2DescriptorSetResult, {}};
            }
            textures.m_debugAttachmentSets2.push_back(std::move(debug2DescriptorSet));
            auto [createDebug3DescriptorSetResult, debug3DescriptorSet] = createImGuiTextureDescriptorSetUnique(
                debug3ImageView
            );
            if (createDebug3DescriptorSetResult != vk::Result::eSuccess) {
                return {createDebug3DescriptorSetResult, {}};
            }
            textures.m_debugAttachmentSets3.push_back(std::move(debug3DescriptorSet));
            auto [createDebug4DescriptorSetResult, debug4DescriptorSet] = createImGuiTextureDescriptorSetUnique(
                debug4ImageView
            );
            if (createDebug4DescriptorSetResult != vk::Result::eSuccess) {
                return {createDebug4DescriptorSetResult, {}};
            }
            textures.m_debugAttachmentSets4.push_back(std::move(debug4DescriptorSet));
        }
        auto handle = m_viewportTextures.allocate(std::move(textures));
        return {vk::Result::eSuccess, handle};
    }

    vk::ResultValue<UniqueViewportTexturesHandle> ImUiContext::createViewportTexturesUnique(
        engine::ViewportResources const &resources) {
        auto [result, handle] = createViewportTextures(resources);
        return {result, {handle, {this}}};
    }

    void ImUiContext::acquireViewportTexture(ViewportTexturesHandle const &texture) {
        m_viewportTextures.acquire(texture);
    }
    void ImUiContext::releaseViewportTexture(ViewportTexturesHandle const &texture) {
        m_viewportTextures.release(texture);
    }

    void ImUiContext::destroyViewportTextures(ViewportTexturesHandle viewportTextures) {
        m_viewportTextures.markForDeletion(viewportTextures);
    }

    vk::ResultValue<engine::ViewportIndices> ImUiContext::getViewportIndices(
        ViewportTexturesHandle viewportTexturesHandle,
        uint32_t index) {
        auto viewportTextures = m_viewportTextures.get(viewportTexturesHandle);
        if (index < viewportTextures->m_colorAttachmentSets.size()) {
            return {vk::Result::eErrorOutOfHostMemory, {}};
        }
        return {vk::Result::eSuccess, engine::ViewportIndices{
            .releaseTextureIndex = reinterpret_cast<uint64_t>(static_cast<VkDescriptorSet>(*viewportTextures->m_colorAttachmentSets[index])),
            .debug1TextureIndex = reinterpret_cast<uint64_t>(static_cast<VkDescriptorSet>(*viewportTextures->m_debugAttachmentSets1[index])),
            .debug2TextureIndex = reinterpret_cast<uint64_t>(static_cast<VkDescriptorSet>(*viewportTextures->m_debugAttachmentSets2[index])),
            .debug3TextureIndex = reinterpret_cast<uint64_t>(static_cast<VkDescriptorSet>(*viewportTextures->m_debugAttachmentSets3[index])),
            .debug4TextureIndex = reinterpret_cast<uint64_t>(static_cast<VkDescriptorSet>(*viewportTextures->m_debugAttachmentSets4[index]))
        }};
    }

    ImUiContext::~ImUiContext() {
        // Читаем текущее значение счетчика
        auto leakedTextures = m_textureCount.load(std::memory_order_relaxed);

        // Если leakedTextures > 0, условие внутри assert станет false и программа упадет,
        // при этом в окне ошибки вы увидите точное количество утекших текстур!
        assert(leakedTextures == 0 && "memory leak: forgot to call destroyImGuiTextureDescriptorSet() for some textures!");

        if (m_imguiContext) {
            auto* ctx_to_destroy = static_cast<ImGuiContext*>(m_imguiContext);

            // 1. Вручную сохраняем старый контекст из нашего TLS
            ImGuiContext* old_context = ImGui::GetCurrentContext();

            // 2. Переключаемся на контекст этого объекта для корректного Shutdown бэкендов
            ImGui::SetCurrentContext(ctx_to_destroy);

            // 3. Завершаем работу бэкендов
            ImGui_ImplVulkan_Shutdown();
            if (m_platform) {
                m_platform->shutdownGuiBackend();
            }
            if (m_window) {
                m_window->setImGuiContext(nullptr);
            }

            // 4. Уничтожаем контекст (ImGui внутри сбросит GImGui в NULL)
            ImGui::DestroyContext(ctx_to_destroy);

            // 5. Возвращаем старый контекст обратно, НО проверяем, не удалили ли мы его только что
            if (old_context == ctx_to_destroy) {
                ImGui::SetCurrentContext(nullptr); // Безопасно обнуляем
            } else {
                ImGui::SetCurrentContext(old_context); // Возвращаем контекст другого окна
            }
        }
    }


}

void vk::UniqueHandleTraits<vk::DescriptorSet, shuttle::ImUiContext>::deleter::destroy(
    DescriptorSet descriptorSet) const {
        m_context->destroyImGuiTextureDescriptorSet(descriptorSet);
}
