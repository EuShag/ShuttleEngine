//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_RENDER_HPP
#define HELLOTRIANGLE_RENDER_HPP
#include "IncludeVulkan.hpp"
#include "TextureCatalog.hpp"
#include "../HostRenderData/HostRenderData.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "BlobLoader/BlobLoader.hpp"
#include "EnvironmentBlobLoader/EnvironmentBlobLoader.hpp"

namespace shuttle_engine{

    template <typename T>
    constexpr T alignUp(T value, T alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    // Packed vertex structs matching the blob binary format exactly (no padding).
    struct PositionAttribute {
        glm::vec3 position;  // 12 bytes, no alignment padding
    };
    struct NormalTangentUvAttribute {
        glm::vec3 normal;   // 12 bytes
        glm::vec2 uv;       // 8 bytes
        glm::vec4 tangent;  // 16 bytes
    };  // total 36 bytes

    struct CameraUniformData {
        glm::mat4 viewMatrix;
        glm::mat4 ProjectionMatrix;
        alignas(16) glm::vec3 cameraPos;
    };

    struct alignas(16) SceneLightingData {
        alignas(16) glm::vec4 ambient;
        uint32_t directionalLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t padding;
    };

    struct DirectionalLightData {
        alignas(16) glm::vec4 direction;
        alignas(16) glm::vec4 color; // xyz - color, w - intensity
        glm::mat4 lightSpaceMatrix;
    };

    struct DirectionalLightLightSpaceMatrixData {
        glm::mat4 lightSpaceMatrix;
    };

    struct VulkanBufferInfo {
        vk::Buffer buffer;
        vk::DeviceSize offset;
    };

    struct VulkanMeshData {
        VulkanBufferInfo vertexPosition;
        VulkanBufferInfo vertexNormalUvTangent;
        VulkanBufferInfo indices;
    };

    struct RenderMeshData {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
        uint32_t firstInstance;
    };

    struct IndirectDraw {
        uint32_t materialIndex;
        uint32_t commandCount;
        uint32_t indirectBufferOffset;
    };

    struct ModelData {
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
    };

    // Raw-byte geometry + draw commands built from BlobSceneData.
    struct MeshData {
        std::vector<uint8_t> positionData;   // raw bytes matching blob layout
        std::vector<uint8_t> attributeData;  // raw bytes
        std::vector<uint8_t> indexData;      // raw bytes

        std::vector<vk::DrawIndexedIndirectCommand> indirectCommands;
        std::vector<ModelData> modelDatas;
        std::vector<IndirectDraw> indirectDrawCalls;
    };

    struct StagingBufferMeshData {
        vk::Buffer stagingBuffer;
        vk::DeviceSize vertexBufferSize;
        vk::DeviceSize positionAttributeVertexBindingBufferOffset;
        vk::DeviceSize normalUvTangentAttributeVertexBindingBufferOffset;
        vk::DeviceSize indexBufferSize;
        vk::DeviceSize indicesBufferOffset;
        vk::DeviceSize indirectCommandsBufferSize;
        vk::DeviceSize indirectCommandsBufferOffset;
        vk::DeviceSize modelDatasBufferSize;
        vk::DeviceSize modelDatasBufferOffset;

        void recordCopyCommandsToBuffer(
            vk::CommandBuffer cmdBuffer,
            vk::Buffer vertexBuffer,
            vk::Buffer indexBuffer,
            vk::Buffer indirectCommandsBuffer,
            vk::Buffer modelDatasBuffer
        ) const;

        std::vector<IndirectDraw> indirectDrawCalls;
    };

    struct DeviceBufferMeshData {
        vk::Buffer vertexBuffer;
        vk::DeviceSize normalUvTangentAttributeVertexBindingBufferOffset;
        vk::Buffer indexBuffer;
        vk::Buffer indirectCommandsBuffer;
        vk::Buffer modelDatasBuffer;
        std::vector<IndirectDraw> indirectDrawCalls;
    };

    struct SceneResourcesOwner {
        std::vector<resources::UniqueAllocatedBuffer> buffers;
        std::vector<resources::UniqueAllocatedImage> images;
        std::vector<vk::UniqueImageView> imageViews;
        vk::UniqueDescriptorPool descriptorPool;
        std::vector<vk::UniqueFramebuffer> framebuffers;
        std::vector<vk::UniqueDescriptorSet> descriptorSets;
        std::vector<vk::UniqueSampler> samplers;
    };

    struct DeviceMeshData {
        resources::UniqueAllocatedBuffer vertexBuffer;
        vk::DeviceSize positionAttributeOffset;
        vk::DeviceSize normalUvTangentAttributeOffset;

        resources::UniqueAllocatedBuffer indexBuffer;
        vk::DeviceSize indexBufferOffset;

        resources::UniqueAllocatedBuffer indirectBuffer;
        vk::DeviceSize indirectBufferOffset;

        resources::UniqueAllocatedBuffer modelSsboBuffer;
        vk::DeviceSize modelSsboBufferOffset;
        vk::UniqueDescriptorSet modelSsboDescriptorSet;

        std::vector<IndirectDraw> indirectDraws;
    };

    struct RenderMaterialData {
        vk::UniqueDescriptorSet materialSet;
        vk::DeviceSize indirectDrawOffset;
        uint32_t commandsCount;
    };

    // Per-material device resources (UBO only; textures live in the global array).
    struct DeviceMaterialInfo {
        resources::UniqueAllocatedBuffer uniformBufferMaterialProperties;
    };

    struct DeviceSceneData {

        SceneLightingData sceneLightingData;
        std::vector<DirectionalLightData> directionalLightDatas;

        struct RenderMaterialData {
            DeviceMaterialInfo deviceMaterialInfo;
            vk::DescriptorSet materialSet;
        };

        // Geometry
        resources::UniqueAllocatedBuffer vertexBuffer;
        vk::DeviceSize positionAttributeOffset{0};
        vk::DeviceSize normalUvTangentAttributeOffset{0};

        resources::UniqueAllocatedBuffer indexBuffer;
        vk::DeviceSize indexBufferOffset{0};

        resources::UniqueAllocatedBuffer indirectDrawBuffer;
        vk::DeviceSize indirectDrawBufferOffset{0};

        std::vector<IndirectDraw> indirectDraws;

        resources::UniqueAllocatedBuffer modelSsbo;
        vk::DescriptorSet modelSsboDescriptorSet;

        // Materials
        std::vector<RenderMaterialData> materials;

        // Global texture array (all scene textures indexed by TextureMetaData order)
        std::vector<resources::UniqueAllocatedImage> textureImages;
        std::vector<vk::UniqueImageView> textureViews;

        // Environment maps
        resources::UniqueAllocatedImage skyboxImage;
        vk::UniqueImageView skyboxView;

        resources::UniqueAllocatedImage irradianceImage;
        vk::UniqueImageView irradianceView;

        resources::UniqueAllocatedImage radianceImage;
        vk::UniqueImageView radianceView;

        vk::DescriptorSet environmentDescriptorSet;

        vk::DescriptorSet materialDescriptorSet;


        vk::UniqueDescriptorPool descriptorPool;
    };

    struct DeviceShadowMapRenderTarget {
        vk::UniqueFramebuffer framebuffer;
        vk::Extent2D extent;
    };

    struct RenderTargets {
        resources::UniqueAllocatedImage depthBufferImage;
        vk::UniqueImageView depthBufferImageView;
        vk::Image colorAttachmentImage;
        vk::UniqueImageView colorAttachmentImageView;
        vk::Extent2D renderTargetExtent;
    };

    struct FrameData {
        resources::UniqueAllocatedImage shadowMapImage;
        vk::UniqueImageView shadowMapImageView;
        vk::Extent2D shadowExtent;

        resources::UniqueAllocatedBuffer cameraUbo;
        resources::UniqueAllocatedBuffer lightInfoUbo;
        resources::UniqueAllocatedBuffer lightSsbo;
        vk::DescriptorSet sceneDataSet;
        resources::UniqueAllocatedBuffer screenshotBuffer;
    };

    struct RenderContext {
        vk::Device device;
        resources::DeviceAllocator allocator;

        vk::UniqueDescriptorPool descriptorPool;
    };

    struct DeviceRendererResources
    {
        // Pipelines
        vk::UniquePipeline mainPipeline;
        vk::UniquePipeline shadowPipeline;
        vk::UniquePipeline skyboxPipeline;

        // Pipeline layouts
        vk::UniquePipelineLayout mainPipelineLayout;
        vk::UniquePipelineLayout shadowPipelineLayout;
        vk::UniquePipelineLayout skyboxPipelineLayout;

        // Descriptor layouts
        vk::UniqueDescriptorSetLayout rendererSetLayout;
        vk::UniqueDescriptorSetLayout environmentSetLayout;
        vk::UniqueDescriptorSetLayout sceneSetLayout;
        vk::UniqueDescriptorSetLayout frameSetLayout;

        // Samplers
        vk::UniqueSampler materialSampler;
        vk::UniqueSampler shadowSampler;
        vk::UniqueSampler nearestSampler;

        // Renderer descriptor set
        vk::DescriptorSet rendererSet;

        // BRDF
        resources::UniqueAllocatedImage brdfLutImage;
        vk::UniqueImageView brdfLutImageView;

        // Fallback textures
        resources::UniqueAllocatedImage fallbackAlbedoImage;
        resources::UniqueAllocatedImage fallbackNormalImage;
        resources::UniqueAllocatedImage fallbackOrmImage;
        resources::UniqueAllocatedImage fallbackEmissionImage;

        vk::UniqueImageView fallbackAlbedoImageView;
        vk::UniqueImageView fallbackNormalImageView;
        vk::UniqueImageView fallbackOrmImageView;
        vk::UniqueImageView fallbackEmissionImageView;
    };

    struct DeviceSceneResources
    {
        // Geometry
        resources::UniqueAllocatedBuffer vertexBuffer;
        resources::UniqueAllocatedBuffer indexBuffer;

        // Materials
        resources::UniqueAllocatedBuffer materialSsbo;

        // Draw data
        resources::UniqueAllocatedBuffer modelSsbo;
        resources::UniqueAllocatedBuffer indirectDrawBuffer;

        std::vector<IndirectDraw> indirectDraws;

        // Textures
        ShuttleEngine::render::TextureCatalog textureCatalog;

        std::vector<resources::UniqueAllocatedImage> textureImages;
        std::vector<vk::UniqueImageView> textureImageViews;

        vk::DescriptorSet sceneSet;
    };

    struct DeviceFrameResources
    {
        resources::UniqueAllocatedBuffer cameraBuffer;
        resources::UniqueAllocatedBuffer lightingInfoBuffer;
        resources::UniqueAllocatedBuffer directionalLightsBuffer;

        resources::UniqueAllocatedImage shadowMapImage;
        vk::UniqueImageView shadowMapImageView;

        resources::UniqueAllocatedBuffer screenshotBuffer;

        vk::DescriptorSet frameSet;
    };

    struct DeviceEnvironmentResources
    {
        resources::UniqueAllocatedImage skyboxImage;
        vk::UniqueImageView skyboxView;

        resources::UniqueAllocatedImage irradianceImage;
        vk::UniqueImageView irradianceView;

        resources::UniqueAllocatedImage radianceImage;
        vk::UniqueImageView radianceView;

        vk::DescriptorSet environmentSet;
    };

    struct SceneFrameRequirements
    {
        uint32_t modelCount;

        uint32_t directionalLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
    };

    struct UploadSceneOutput {
        DeviceSceneResources deviceSceneResources;
        SceneFrameRequirements sceneFrameRequirements;
    };

    struct HostFrameResources
    {
        resources::UniqueAllocatedBuffer uploadBuffer;
        void* mappedMemory;

        // Layout
        vk::DeviceSize cameraOffset;
        vk::DeviceSize lightingInfoOffset;
        vk::DeviceSize directionalLightsOffset;
        vk::DeviceSize modelDataOffset;

        // Typed access
        CameraUniformData* camera;
        SceneLightingData* lightingInfo;

        std::span<DirectionalLightData> directionalLights;
        std::span<ModelData> modelDatas;
    };

    struct FrameRenderSettings
    {
        bool renderShadows = true;
        bool renderSkybox = true;

        bool makeScreenshot = false;

        bool renderDebugGeometry = false;
        bool renderDebugLights = false;

        bool renderTransparency = true;
    };



    class PbrRender {
    public:

        static vk::ResultValue<PbrRender> create(vk::Device device, vk::ImageLayout finalLayout, vk::Queue transferQueue,
                                          vk::CommandPool transferCommandPool,
                                          resources::DeviceAllocator const &allocator);

        vk::ResultValue<DeviceSceneData> uploadScene(
            const BlobSceneData& blob,
            const assets::BlobEnvironmentData* environment,
            vk::Queue transferQueue,
            vk::Device device,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator
        );

        static vk::Result updateSceneData(
            resources::DeviceAllocator const& allocator,
            DeviceSceneData& sceneData,
            FrameData& frameData,
            glm::mat4 const& viewMatrix,
            glm::mat4 const& projectionMatrix,
            glm::mat4 const& shortProjectionMatrix,
            glm::vec3 const& cameraPos
        );

        [[nodiscard]] vk::ResultValue<std::vector<RenderTargets>> createRenderTargets(
            vk::Device device,
            resources::DeviceAllocator const& allocator,
            std::vector<vk::Image> const& targetImages,
            vk::Extent2D renderTargetExtent
        ) const;

        [[nodiscard]] vk::ResultValue<std::vector<FrameData>> createFrameDatas(
            vk::Device device,
            resources::DeviceAllocator const& allocator,
            vk::Extent2D shadowMapExtent,
            vk::Extent2D renderTargetExtent,
            vk::DescriptorPool descriptorPool,
            uint32_t frameCount
        ) const;

        void recordRenderFrameCommands(
            DeviceSceneData const& sceneData,
            vk::CommandBuffer cmd,
            FrameData const& frameData,
            RenderTargets const& targets,
            std::function<void(vk::CommandBuffer)> const& additionalCommands,
            bool needsMakeScreenshot) const;

        PbrRender(const PbrRender&) = delete;
        PbrRender& operator=(const PbrRender&) = delete;

        PbrRender(PbrRender&&) = default;
        PbrRender& operator=(PbrRender&&) = default;

        ~PbrRender() = default;

    private:
        vk::Result initPbrMaterialSetLayout(vk::Device device);
        vk::Result initPbrEnvironmentSetLayout(vk::Device device);
        vk::Result initSceneDataSetLayout(vk::Device device);
        vk::Result initMainPipelineLayout(vk::Device device);
        vk::Result initShadowPipelineLayout(vk::Device device);

        vk::Result initSkyboxPipelineLayout(vk::Device device);

        vk::Result initMainPipeline(vk::Device device);
        vk::Result initShadowPipeline(vk::Device device);

        vk::Result initSkyboxPipeline(vk::Device device);

        vk::Result initSamplers(vk::Device device);
        vk::Result initSamplerDescriptorSet(vk::Device device);
        vk::Result initSamplerDescriptorSetLayout(vk::Device device);
        vk::Result initModelDataSetLayout(vk::Device device);

        static MeshData prepareMeshData(BlobSceneData const& blob);

        static vk::ResultValue<StagingBufferMeshData> prepareStagingBufferMeshData(
            MeshData const& meshData,
            resources::DeviceAllocator const& deviceAllocator,
            resources::AllocatedBuffer stagingBuffer,
            vk::DeviceSize& stagingBufferOffset
        );

        vk::Result initBuiltinResources(
            vk::Device device,
            vk::Queue transferQueue,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator
        );


        static vk::ResultValue<DeviceMeshData> prepareDeviceMeshData(
            const StagingBufferMeshData& stagingInfo,
            resources::DeviceAllocator const& deviceAllocator);

        static void fillDescriptorSet(
            vk::Device device,
            vk::DescriptorSet materialSet,
            vk::Buffer propertiesUbo,
            std::array<vk::ImageView, 5> const& textureViews);

        PbrRender() = default;

        vk::UniquePipeline mainPipeline;
        vk::UniquePipeline shadowPipeline;
        vk::UniquePipeline skyboxPipeline;

        vk::UniquePipelineLayout mainPipelineLayout;
        vk::UniquePipelineLayout shadowPipelineLayout;
        vk::UniquePipelineLayout skyboxPipelineLayout;

        vk::UniqueDescriptorSetLayout pbrSceneDataSetLayout;
        vk::UniqueDescriptorSetLayout pbrMaterialSetLayout;
        vk::UniqueDescriptorSetLayout modelSetLayout;
        vk::UniqueDescriptorSetLayout pbrEnvironmentSetLayout;

        vk::UniqueSampler shadowSampler;
        vk::UniqueSampler materialSampler;
        vk::UniqueDescriptorPool samplerDescriptorPool;
        vk::UniqueDescriptorSetLayout samplersSetLayout;
        vk::DescriptorSet samplersSet;

        resources::UniqueAllocatedImage brdfLutImage;
        vk::UniqueImageView brdfLutImageView;

        resources::UniqueAllocatedImage fallbackAlbedoImage;
        vk::UniqueImageView fallbackAlbedoImageView;

        resources::UniqueAllocatedImage fallbackNormalImage;
        vk::UniqueImageView fallbackNormalImageView;

        resources::UniqueAllocatedImage fallbackOrmImage;
        vk::UniqueImageView fallbackOrmImageView;

        resources::AllocatedImage fallbackEmissiveImage;
        vk::UniqueImageView fallbackEmissiveImageView;

        resources::AllocatedImage fallbackHeightImage;
        vk::UniqueImageView fallbackHeightImageView;    };
}

#endif //HELLOTRIANGLE_RENDER_HPP
