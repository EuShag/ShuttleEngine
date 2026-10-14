#ifndef HELLOTRIANGLE_UIRENDER_HPP
#define HELLOTRIANGLE_UIRENDER_HPP

#include <atomic>

#include "IncludeVulkan.hpp"
#include "Engine/Handle.hpp"
#include "Engine/Resource.hpp"
#include "Engine/ViewportResources.hpp"
#include "PAL/Platform.hpp"
#include "PAL/Common/Window/WindowBase.hpp"

namespace shuttle {
    namespace engine {
        struct ViewportResources;
    }

    class ImUiContext;
}

template<>
class vk::UniqueHandleTraits<vk::DescriptorSet, shuttle::ImUiContext> {
public:
    struct deleter {
        deleter(shuttle::ImUiContext const& context) : m_context(&context) {}
        void destroy(DescriptorSet descriptorSet) const;
    private:
        shuttle::ImUiContext const* m_context;
    };
};

namespace shuttle
{
    using ImGuiUniqueDescriptorSet = vk::UniqueHandle<vk::DescriptorSet, ImUiContext>;

    struct IUiPainter {
        virtual void drawUi(uint32_t frameIndex) = 0;
        virtual ~IUiPainter() = default;
    };

    struct ViewportTexturesHandleTag {};

    using ViewportTexturesHandle = engine::TypedHandle<ViewportTexturesHandleTag>;

    struct ViewportTextures {
        std::vector<ImGuiUniqueDescriptorSet> m_colorAttachmentSets;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets1;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets2;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets3;
        std::vector<ImGuiUniqueDescriptorSet> m_debugAttachmentSets4;
    };

    struct ViewportTexturesDeleter {

        ViewportTexturesDeleter() = default;
        ViewportTexturesDeleter(ImUiContext* context) : m_imguiContext(context) {};

        void operator()(ViewportTexturesHandle) const;
        ImUiContext* m_imguiContext = nullptr;
    };

    template<> struct engine::ResourceTraits<ViewportTextures> { using handleType = ViewportTexturesHandle; };
    using UniqueViewportTexturesHandle = engine::UniqueHandle<ViewportTexturesHandle, ViewportTexturesDeleter>;

    class ImUiContext {
    public:
        static vk::ResultValue<ImUiContext> create(
            pal::WindowBase &window,
            vk::Instance instance,
            vk::PhysicalDevice physicalDevice,
            vk::Device device,
            vk::detail::DispatchLoaderDynamic const &dispatcher,
            uint32_t queueFamilyIndex,
            vk::Queue queue, uint32_t imageCount);

        void drawUi(IUiPainter &painter, uint32_t frameIndex) const;

        void writeRenderCommands(vk::CommandBuffer cmd) const;

        [[nodiscard]] vk::ResultValue<vk::DescriptorSet> createImGuiTextureDescriptorSet(vk::ImageView imageView) const;

        [[nodiscard]] vk::ResultValue<ImGuiUniqueDescriptorSet> createImGuiTextureDescriptorSetUnique(vk::ImageView imageView) const;

        void destroyImGuiTextureDescriptorSet(vk::DescriptorSet descriptorSet) const;

        [[nodiscard]] vk::ResultValue<ViewportTexturesHandle> createViewportTextures(engine::ViewportResources const& resources);
        [[nodiscard]] vk::ResultValue<UniqueViewportTexturesHandle> createViewportTexturesUnique(engine::ViewportResources const& resources);

        void acquireViewportTexture(ViewportTexturesHandle const& texture);
        void releaseViewportTexture(ViewportTexturesHandle const& texture);

        void destroyViewportTextures(ViewportTexturesHandle viewportTextures);

        [[nodiscard]] vk::ResultValue<engine::ViewportIndices> getViewportIndices(ViewportTexturesHandle viewportTextures, uint32_t index);

        ImUiContext() = default;

        ImUiContext(const ImUiContext&) = delete;
        ImUiContext& operator=(const ImUiContext&) = delete;

        ImUiContext(ImUiContext&& other) noexcept
            : uiDescriptorPool(std::move(other.uiDescriptorPool))
            , m_imguiContext(other.m_imguiContext)
            , m_platform(other.m_platform)
        {
            other.m_platform = nullptr;
            other.m_imguiContext = nullptr;
        }

        ImUiContext& operator=(ImUiContext&& other) noexcept
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

        ~ImUiContext();

    private:
        engine::SlotMapStorage<ViewportTextures> m_viewportTextures;
        vk::UniqueDescriptorPool uiDescriptorPool{};
        void* m_imguiContext = nullptr;
        pal::Platform* m_platform = nullptr; // Сохраняем ссылку на платформу для зачистки бэкенда
        pal::WindowBase* m_window = nullptr; // Сохраняем ссылку на окно для зачистки бэкенда
        mutable std::atomic_uint32_t m_textureCount{};
    };
} // namespace shuttle

#endif // HELLOTRIANGLE_UIRENDER_HPP
