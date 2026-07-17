//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_RENDER_HPP
#define HELLOTRIANGLE_RENDER_HPP
#include "IncludeVulkan.hpp"
#include "../HostRenderData/HostRenderData.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"

namespace shuttle_engine{

    template <typename T>
    constexpr T alignUp(T value, T alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    struct PositionAttribute {
        alignas(16) glm::vec3 position;
    };
    struct NormalTangentUvAttribute {
        alignas(16) glm::vec3 normal;
        alignas(16) glm::vec2 uv;
        alignas(16) glm::vec4 tangent;
    };

    struct CameraUniformData {
        glm::mat4 viewProj;
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
        alignas(16) glm::vec3 direction;
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

    struct MeshData {
        std::vector<PositionAttribute> positionData; // Все позиции подряд
        std::vector<NormalTangentUvAttribute> normalUvTangentData; // Все нормали, UV и тангенсы подряд
        std::vector<uint32_t> indices; // Все вершины

        std::vector<vk::DrawIndexedIndirectCommand> indirectCommands; // Команды для отрисовки всех экземпляров этого меша
        std::vector<ModelData> modelDatas; // Массив матриц трансформации для всех экземпляров этого меша

        std::vector<IndirectDraw> indirectDrawCalls; // Данные для вызова vk::CommandBuffer::drawIndexedIndirect()
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

    struct StagingBufferMaterialOffsets {
        vk::DeviceSize propertiesOffset;
        vk::DeviceSize albedoOffset;
        vk::DeviceSize normalOffset;
        vk::DeviceSize ormOffset;
        vk::DeviceSize emissionOffset;
        vk::DeviceSize heightOffset;
    };

    struct PreparedHostMaterialData {
        static constexpr std::array<uint8_t, 4> defaultAlbedoValue{255, 255, 255, 255}; // Белый
        static constexpr std::array<uint8_t, 4> defaultNormalValue{128, 128, 255, 255}; // Нейтральный синий (для нормалей)
        static constexpr std::array<uint8_t, 4> defaultOrmValue{255, 255, 0, 255};      // AO = 1.0, Roughness = 1.0, Metallic = 0.0
        static constexpr std::array<uint8_t, 4> defaultEmissiveValue{0, 0, 0, 255};     // Черный (нет свечения)
        static constexpr std::array<uint8_t, 4> defaultHeightValue{255, 255, 255, 255};  // Белый

        HostMaterialProperties hostMaterialProperties;
        HostImageData albedoHostImageData;
        HostImageData normalHostImageData;
        HostImageData ormHostImageData;
        HostImageData emissionHostImageData;
        HostImageData heightHostImageData;

        vk::DeviceSize stagingBufferRequiredSize;
        StagingBufferMaterialOffsets stagingBufferOffsets;

        void calculateOffsetsAndSize();
    };

    struct StagingBufferImageInfo {
        vk::Extent2D imageSize;
        vk::DeviceSize stagingBufferOffset{};
        vk::Format imageFormat;
        std::vector<MipInfo> mipLevels;
    };

    struct StagingBufferMaterialInfo {
        vk::DeviceSize propertiesOffset{};
        StagingBufferImageInfo albedoInfo;
        StagingBufferImageInfo normalInfo;
        StagingBufferImageInfo ormInfo;
        StagingBufferImageInfo emissionInfo;
        StagingBufferImageInfo heightInfo;

        void prepareCopyCommandsToBuffer(
            vk::CommandBuffer commandBuffer,
            vk::Buffer stagingBuffer,
            vk::Buffer propertiesUboBuffer,
            vk::Image dstAlbedoImage,
            vk::Image dstNormalImage,
            vk::Image dstOrmImage,
            vk::Image dstEmissionImage,
            vk::Image dstHeightImage
        ) const;
    };

    struct DeviceMaterialInfo {
        resources::UniqueAllocatedBuffer uniformBufferMaterialProperties;

        resources::UniqueAllocatedImage albedoImage;
        resources::UniqueAllocatedImage normalImage;
        resources::UniqueAllocatedImage ormImage;
        resources::UniqueAllocatedImage emissionImage;
        resources::UniqueAllocatedImage heightImage;

        vk::UniqueImageView albedoTextureView;
        vk::UniqueImageView normalTextureView;
        vk::UniqueImageView ormTextureView;
        vk::UniqueImageView emissionTextureView;
        vk::UniqueImageView heightTextureView;

        void recordMaterialImagesBarriers(
            vk::CommandBuffer commandBuffer,
            uint32_t albedoMipLevelsCount,
            uint32_t normalMipLevelsCount,
            uint32_t ormMipLevelsCount,
            uint32_t emissionMipLevelsCount,
            uint32_t heightMipLevelsCount
            ) const;
    };

    struct DeviceSceneData {

        SceneLightingData sceneLightingData;
        std::vector<DirectionalLightData> directionalLightDatas;

        struct RenderMaterialData {
            DeviceMaterialInfo deviceMaterialInfo;
            vk::DescriptorSet materialSet;
        };

        // --- 4. Геометрия ---
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

        // --- 5. Материалы (Список всех Set 1) ---
        std::vector<RenderMaterialData> materials;

        // --- 6. Основной DescriptorPool ---
        vk::UniqueDescriptorPool descriptorPool;
    };

    struct DeviceShadowMapRenderTarget {
        vk::UniqueFramebuffer framebuffer;
        vk::Extent2D extent;
    };

    struct RenderTargets {
        // Depth + color buffers
        resources::UniqueAllocatedImage depthBufferImage;
        vk::UniqueImageView depthBufferImageView;
        vk::UniqueImageView colorAttachmentImageView;
        vk::UniqueFramebuffer mainRenderPassFramebuffer;
        vk::Extent2D renderTargetExtent;
    };

    struct FrameData {
        // Shadow resources
        resources::UniqueAllocatedImage shadowMapImage;
        vk::UniqueImageView shadowMapImageView;
        vk::UniqueFramebuffer shadowRenderPassFramebuffer;
        vk::Extent2D shadowExtent;

        // Scene Data
        resources::UniqueAllocatedBuffer cameraUbo;
        resources::UniqueAllocatedBuffer lightInfoUbo;
        resources::UniqueAllocatedBuffer lightSsbo;
        vk::DescriptorSet sceneDataSet;
    };

    class PbrRender {
    public:
        static vk::ResultValue<PbrRender> create(vk::Device device, vk::ImageLayout finalColorLayout);

        vk::ResultValue<DeviceSceneData> uploadScene(
            HostSceneData&& hostSceneData,
            vk::Queue transferQueue,
            vk::Device device,
            vk::CommandPool transferCommandPool,
            resources::DeviceAllocator const& allocator
        );

        // Наш новый, красивый и умный метод обновления
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
            vk::DescriptorPool descriptorPool,
            uint32_t frameCount
        ) const;

        // Обновленная функция записи команд — чистая от матриц!
        void recordRenderFrameCommands(
            DeviceSceneData const& sceneData,
            vk::CommandBuffer cmd,
            FrameData const& frameData,
            RenderTargets const& targets,
            std::function<void(vk::CommandBuffer)> const& additionalCommands) const;

        vk::RenderPass getMainRenderPass(){ return *mainRenderPass; }

        // Запрещаем копирование, разрешаем перемещение
        PbrRender(const PbrRender&) = delete;
        PbrRender& operator=(const PbrRender&) = delete;

        PbrRender(PbrRender&&) = default;
        PbrRender& operator=(PbrRender&&) = default;

        ~PbrRender() = default;

    private:
        vk::Result initMainRenderPass(vk::Device device, vk::ImageLayout finalColorLayout);
        vk::Result initShadowRenderPass(vk::Device device);
        vk::Result initPbrMaterialSetLayout(vk::Device device);
        vk::Result initSceneDataSetLayout(vk::Device device);
        vk::Result initMainPipelineLayout(vk::Device device);
        vk::Result initShadowPipelineLayout(vk::Device device);
        vk::Result initMainPipeline(vk::Device device);
        vk::Result initShadowPipeline(vk::Device device);
        vk::Result initSamplers(vk::Device device);
        vk::Result initSamplerDescriptorSet(vk::Device device);
        vk::Result initSamplerDescriptorSetLayout(vk::Device device);
        vk::Result initModelDataSetLayout(vk::Device device);

        static MeshData prepareHostMeshData(
            HostSceneData const& sceneData
        );

        static vk::ResultValue<StagingBufferMeshData> prepareStagingBufferMeshData(
            MeshData const& meshData,
            resources::DeviceAllocator const& deviceAllocator,
            resources::AllocatedBuffer stagingBuffer,
            vk::DeviceSize& stagingBufferOffset
        );

        static vk::ResultValue<DeviceMeshData> prepareDeviceMeshData(
            const StagingBufferMeshData& stagingInfo,
            resources::DeviceAllocator const& deviceAllocator);

        static PreparedHostMaterialData prepareHostMaterialData(
            HostMaterialData const& material
        );

        static vk::ResultValue<StagingBufferMaterialInfo> prepareStagingBufferMaterialInfo(
            PreparedHostMaterialData&& preparedMaterialData,
            resources::DeviceAllocator const& deviceAllocator,
            resources::AllocatedBuffer const & stagingBuffer,
            vk::DeviceSize& stagingBufferOffset);

        static vk::ResultValue<DeviceMaterialInfo> prepareDeviceMaterialInfo(
            const StagingBufferMaterialInfo& stagingInfo,
            vk::Device device,
            resources::DeviceAllocator const& deviceAllocator);

        static void fillDescriptorSet(
            vk::Device device,
            vk::DescriptorSet materialSet,
            const DeviceMaterialInfo& info);


        PbrRender() = default;

        vk::UniqueRenderPass mainRenderPass;
        vk::UniqueRenderPass shadowRenderPass;

        vk::UniquePipeline mainPipeline;
        vk::UniquePipeline shadowPipeline;

        vk::UniquePipelineLayout mainPipelineLayout;
        vk::UniquePipelineLayout shadowPipelineLayout;

        vk::UniqueDescriptorSetLayout pbrSceneDataSetLayout;
        vk::UniqueDescriptorSetLayout pbrMaterialSetLayout;
        vk::UniqueDescriptorSetLayout modelSetLayout;

        vk::UniqueSampler shadowSampler;
        vk::UniqueSampler materialSampler;
        vk::UniqueDescriptorPool samplerDescriptorPool;
        vk::UniqueDescriptorSetLayout samplersSetLayout;
        vk::DescriptorSet samplersSet;
    };
}

#endif //HELLOTRIANGLE_RENDER_HPP