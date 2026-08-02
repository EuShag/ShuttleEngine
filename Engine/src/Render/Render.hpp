//
// Created by Shagu on 27.07.2026.
//
#ifndef SHUTTLEENGINE_RENDER_HPP
#define SHUTTLEENGINE_RENDER_HPP
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <Assets/Formats/Lighting.hpp>
#include <Assets/Formats/Scene.hpp>

#include "TextureCatalog.hpp"
#include <filesystem>

#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Render.hpp"

#include "SetBindings.hpp"

namespace shuttle::assets::formats::scene
{
struct SceneNode;
}

namespace shuttle::engine::render
{

struct RenderTarget
{
    vk::Image image{};
    vk::UniqueImageView imageView{};
};

struct RenderTargets
{
    vk::Extent2D renderTargetExtent{};

    vk::Image colorAttachmentImage{};
    vk::UniqueImageView colorAttachmentImageView{};

    uint32_t swapchainImageIndex{};
};

struct DirectionalLightData
{
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float intensity{1.0f};

    glm::vec3 color{1.0f};
    uint32_t castShadows{1};
};

struct SceneLightingData
{
    uint32_t directionalLightCount{};
    uint32_t pointLightCount{};
    uint32_t spotLightCount{};
};

inline constexpr uint32_t MaxShadowCascades = 4;
inline constexpr uint32_t MaxHiZMipCount = 16;
inline constexpr uint32_t MaxBindlessTextures = 2048;
inline constexpr uint32_t MaxFrustums = 5;

struct alignas(16) RenderStatistics
{
    uint32_t totalDrawables{};

    uint32_t frustumRejected{};

    uint32_t firstOcclusionRejected{};
    uint32_t secondOcclusionRejected{};

    uint32_t candidateDrawables{};
    uint32_t visibleCandidates{};

    uint32_t visibleDrawables{};

    uint32_t totalDrawCalls{};
    uint32_t activeDrawCalls{};

    uint32_t renderedInstances{};
    uint32_t renderedTriangles{};

    uint32_t reserved0{};
    uint32_t reserved1{};
    uint32_t reserved2{};
};

struct alignas(16) MeshRange
{
    uint32_t firstInstance{};
    uint32_t instanceCount{};
    uint32_t commandIndex{};
    uint32_t reserved{};
};

static_assert(sizeof(MeshRange) == 16);

struct SceneLightingInfo
{
    uint32_t directionalLightCount{};
    uint32_t pointLightCount{};
    uint32_t spotLightCount{};

    uint32_t reserved{};
};

inline constexpr uint32_t FrustumPlaneCount = 6;

enum class FrustumPlaneIndex : uint32_t
{
    eLeft = 0,
    eRight = 1,

    eBottom = 2,
    eTop = 3,

    eNear = 4,
    eFar = 5
};

struct alignas(16) FrustumPlanesData
{
    glm::vec4 planes[FrustumPlaneCount];
};

struct CascadeSetupPushConstants
{
    glm::vec4 lightDirection{};

    float shadowDistance = 250.0f;
    float splitLambda = 0.85f;

    uint32_t cascadeCount = 4;
    uint32_t shadowMapResolution = 4096;

    float depthPadding = 100.0f;

    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
    uint32_t reserved2 = 0;
};

static_assert(sizeof(CascadeSetupPushConstants) <= 128);

struct GtaoPushConstants
{
    float radius = 1.0f;
    float intensity = 1.0f;
    float power = 1.5f;

    uint32_t sampleCount = 8;

    float maxRadiusPixels = 32.0f;

    float depthBias = 0.02f;

    uint32_t reserved0 = 0;
    uint32_t reserved1 = 0;
};

struct GtaoDenoisePushConstants
{
    float depthSigma = 0.1f;
    float aoSigma = 0.25f;

    int32_t kernelRadius = 2;

    int32_t reserved0 = 0;
};

static_assert(sizeof(GtaoPushConstants) <= 128);

static_assert(sizeof(FrustumPlanesData) == sizeof(glm::vec4) * FrustumPlaneCount);

struct alignas(16) CascadeShadowData
{
    glm::mat4 lightViewProjection;

    glm::vec4 cascadeSphere;
    // xyz = center
    // w   = radius
};

static_assert(sizeof(CascadeShadowData) == 80);

struct alignas(16) DirectionalShadowData
{
    CascadeShadowData cascades[MaxShadowCascades];

    glm::vec4 cascadeSplits;
    // x = cascade 0 end
    // y = cascade 1 end
    // z = cascade 2 end
    // w = cascade 3 end
};

static_assert(sizeof(DirectionalShadowData) % 16 == 0);

struct RenderContext
{
    vk::Device device;
    resources::DeviceAllocator allocator;

    vk::Format swapchainColorFormat;
    vk::Format swapchainDepthFormat;

    vk::UniqueDescriptorPool descriptorPool;
};

struct LocalTransformData
{
    glm::vec3 translation;
    uint32_t padding;
    glm::quat rotation;
    glm::vec3 scale;
    uint32_t padding1;
};

struct alignas(16) SceneInfo
{
    uint32_t drawableObjectCount{};
    uint32_t transformCount{};
    uint32_t directionalLightCount{};
    uint32_t directionalShadowCasterCount{};
    uint32_t materialCount{};
    uint32_t textureCount{};
    uint32_t reserved0{};
    uint32_t reserved1{};
};

struct alignas(16) FrameInfo
{
    // ============================================================
    // Camera
    // ============================================================

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

    glm::mat4 viewProjectionMatrix;
    glm::mat4 previousViewProjectionMatrix;

    glm::vec4 cameraPosition;

    // ============================================================
    // Resolution
    // ============================================================

    glm::vec2 renderResolution;
    glm::vec2 invRenderResolution;

    glm::vec2 displayResolution;
    glm::vec2 invDisplayResolution;

    // ============================================================
    // Timing
    // ============================================================

    float deltaTime;
    float elapsedTime;

    uint32_t frameIndex;
    uint32_t drawableCount;

    // ============================================================
    // Camera Parameters
    // ============================================================

    float nearPlane;
    float farPlane;

    float exposure;
    float gamma;

    uint32_t frustumCount;
    uint32_t shadowCascadeCount;
};

struct HostSceneData
{
    // Scene graph
    std::vector<assets::formats::scene::SceneNode> nodes;
    std::vector<assets::formats::scene::Transform> transforms;
    std::vector<assets::formats::scene::NodeLevelRange> levels;

    // Drawables
    std::vector<assets::formats::scene::GpuDrawableObject> drawableObjects;

    // Lighting
    std::vector<assets::formats::lighting::DirectionalLight> directionalLights;
    std::vector<assets::formats::lighting::PointLight> pointLights;
    std::vector<assets::formats::lighting::SpotLight> spotLights;

    SceneInfo sceneInfo{};

    // Dirty state
    bool nodesDirty{};
    bool transformsDirty{};
    bool drawableObjectsDirty{};
    bool lightsDirty{};
};

struct DeviceRendererResources
{
    // ============================================================
    // Pipeline Layouts
    // ============================================================

    vk::UniquePipelineLayout pipelineLayout;

    // ============================================================
    // Graphics Pipelines
    // ============================================================

    vk::UniquePipeline occluderPipeline;
    vk::UniquePipeline visibleDepthPipeline;
    vk::UniquePipeline shadowPipeline;
    vk::UniquePipeline mainPipeline;
    vk::UniquePipeline skyboxPipeline;

    // ============================================================
    // Compute Pipelines
    // ============================================================

    vk::UniquePipeline sceneUpdatePipeline;
    vk::UniquePipeline universalFrustumCullPipeline;
    vk::UniquePipeline linearDepthPipeline;
    vk::UniquePipeline hizBuildPipeline;
    vk::UniquePipeline gtaoPipeline;
    vk::UniquePipeline gtaoDenoisePipeline;
    vk::UniquePipeline occlusionCullPass1Pipeline;
    vk::UniquePipeline occlusionCullPass2Pipeline;
    vk::UniquePipeline prefixScanPipeline;
    vk::UniquePipeline instanceResolvePipeline;
    vk::UniquePipeline cascadeSetupPipeline;
    vk::UniquePipeline occluderResolvePipeline;
    vk::UniquePipeline visibleDepthResolvePipeline;

    // ============================================================
    // Descriptor Pool
    // ============================================================

    vk::UniqueDescriptorPool descriptorPool;

    // ============================================================
    // Descriptor Set Layouts
    // ============================================================

    vk::UniqueDescriptorSetLayout rendererSetLayout;
    vk::UniqueDescriptorSetLayout environmentSetLayout;
    vk::UniqueDescriptorSetLayout sceneSetLayout;
    vk::UniqueDescriptorSetLayout frameSetLayout;

    // ============================================================
    // Samplers
    // ============================================================

    vk::UniqueSampler materialSampler;
    vk::UniqueSampler shadowSampler;
    vk::UniqueSampler nearestSampler;

    // ============================================================
    // Renderer Descriptor Set
    // ============================================================

    vk::DescriptorSet rendererSet;

    // ============================================================
    // BRDF LUT
    // ============================================================

    resources::UniqueAllocatedImage brdfLutImage;
    vk::UniqueImageView brdfLutImageView;

    // ============================================================
    // Fallback Textures
    // ============================================================

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
    resources::UniqueAllocatedBuffer sceneInfoBuffer;

    // Geometry
    resources::UniqueAllocatedBuffer positionBuffer;
    resources::UniqueAllocatedBuffer attributeBuffer;
    resources::UniqueAllocatedBuffer indexBuffer;
    resources::UniqueAllocatedBuffer meshBuffer;

    // Lights
    resources::UniqueAllocatedBuffer directionalLightsBuffer;

    // Materials
    resources::UniqueAllocatedBuffer materialSsbo;

    // Textures
    std::vector<resources::UniqueAllocatedImage> textureImages;
    std::vector<vk::UniqueImageView> textureImageViews;
    TextureCatalog textureCatalog;

    // Scene graph
    resources::UniqueAllocatedBuffer sceneNodeBuffer;
    resources::UniqueAllocatedBuffer sceneLevelBuffer;
    resources::UniqueAllocatedBuffer gpuDrawableObjectsBuffer;
    resources::UniqueAllocatedBuffer sceneTransformBuffer;

    vk::DescriptorSet sceneSet;
};

struct DeviceDrawListResources
{
    // ============================================================
    // Draw Commands
    // ============================================================

    resources::UniqueAllocatedBuffer indirectCommandsBuffer;
    resources::UniqueAllocatedBuffer drawCountBuffer;

    resources::UniqueAllocatedBuffer meshRangesBuffer;
    resources::UniqueAllocatedBuffer meshWriteCountersBuffer;

    // ============================================================
    // Instance Resolve
    // ============================================================

    resources::UniqueAllocatedBuffer instanceRemapBuffer;
};

struct DeviceFrameResources
{
    // ============================================================
    // Descriptor Set
    // ============================================================

    vk::DescriptorSet occluderFrameSet{};
    vk::DescriptorSet visibleDepthFrameSet{};
    vk::DescriptorSet shadowFrameSet{};
    vk::DescriptorSet mainFrameSet{};

    // ============================================================
    // Frame Data
    // ============================================================

    resources::UniqueAllocatedBuffer frameInfoBuffer;
    resources::UniqueAllocatedBuffer frustumPlanesBuffer;
    resources::UniqueAllocatedBuffer directionalShadowDataBuffer;

    // ============================================================
    // Scene Update
    // ============================================================

    resources::UniqueAllocatedBuffer worldTransformsBuffer;

    // ============================================================
    // Frustum Culling
    // ============================================================

    resources::UniqueAllocatedBuffer candidateIndicesBuffer;
    resources::UniqueAllocatedBuffer candidateCountBuffer;

    // ============================================================
    // Occlusion Pass #1
    // ============================================================

    resources::UniqueAllocatedBuffer visibleCandidateIndicesBuffer;
    resources::UniqueAllocatedBuffer visibleCandidateCountBuffer;

    // ============================================================
    // Occlusion Pass #2
    // ============================================================

    resources::UniqueAllocatedBuffer visibilityFlagsBuffer;
    resources::UniqueAllocatedBuffer chosenMeshIdsBuffer;

    // ============================================================
    // Draw Lists
    // ============================================================

    DeviceDrawListResources occluderDrawList;
    DeviceDrawListResources visibleDepthDrawList;
    DeviceDrawListResources mainDrawList;
    DeviceDrawListResources shadowDrawList;

    // ============================================================
    // Statistics
    // ============================================================

    resources::UniqueAllocatedBuffer renderStatisticsBuffer;

    // ============================================================
    // Visibility Masks
    // ============================================================

    resources::UniqueAllocatedBuffer visibilityMasksBuffer;

    // ============================================================
    // Depth
    // ============================================================

    resources::UniqueAllocatedImage depthImage;
    vk::UniqueImageView depthImageView;

    // ============================================================
    // Linear Depth
    // ============================================================

    resources::UniqueAllocatedImage linearDepthImage;

    vk::UniqueImageView linearDepthImageView;
    vk::UniqueImageView linearDepthStorageImageView;

    // ============================================================
    // Hi-Z
    // ============================================================

    resources::UniqueAllocatedImage hizImage;

    vk::UniqueImageView hizFullView;

    std::vector<vk::UniqueImageView> hizMipViews;
    std::vector<vk::UniqueImageView> hizStorageMipViews;

    resources::UniqueAllocatedBuffer hizCountersBuffer;

    // ============================================================
    // GTAO
    // ============================================================

    resources::UniqueAllocatedImage gtaoImage;

    vk::UniqueImageView gtaoImageView;
    vk::UniqueImageView gtaoStorageImageView;

    resources::UniqueAllocatedImage gtaoFilteredImage;

    vk::UniqueImageView gtaoFilteredImageView;
    vk::UniqueImageView gtaoFilteredStorageImageView;

    // ============================================================
    // Cascaded Shadows
    // ============================================================

    resources::UniqueAllocatedImage shadowMapImage;
    vk::UniqueImageView shadowMapImageView;
};

struct DeviceEnvironmentResources
{
    resources::UniqueAllocatedImage skyboxImage;
    vk::UniqueImageView skyboxImageView;

    resources::UniqueAllocatedImage irradianceImage;
    vk::UniqueImageView irradianceImageView;

    resources::UniqueAllocatedImage radianceImage;
    vk::UniqueImageView radianceImageView;

    vk::DescriptorSet environmentSet;
};

struct SceneFrameRequirements
{
    uint32_t transformCount{};

    uint32_t drawableObjectCount{};
    uint32_t meshCount{};

    uint32_t directionalShadowCasterCount{};
    uint32_t pointShadowCasterCount{};
    uint32_t spotShadowCasterCount{};
};

struct UploadSceneOutput
{
    HostSceneData hostSceneData;
    DeviceSceneResources deviceSceneResources;
    SceneFrameRequirements sceneFrameRequirements;
};

struct HostFrameData
{
    // ============================================================
    // Upload Buffer
    // ============================================================
    resources::UniqueAllocatedBuffer uploadBuffer;

    void* mappedMemory{};

    // ============================================================
    // Offsets
    // ============================================================
    vk::DeviceSize frameInfoOffset{};
    vk::DeviceSize frustumPlanesOffset{};
    vk::DeviceSize lightingInfoOffset{};
    vk::DeviceSize directionalLightsOffset{};
    vk::DeviceSize directionalShadowDataOffset{};
    vk::DeviceSize localTransformOffset{};

    // ============================================================
    // UBO / SSBO Views
    // ============================================================

    FrameInfo* frameInfo{};
    FrustumPlanesData* frustumPlanes{};
    SceneLightingData* lightingInfo{};
    DirectionalShadowData* directionalShadowData{};

    // ============================================================
    // Arrays
    // ============================================================

    std::span<DirectionalLightData> directionalLights;
    std::span<LocalTransformData> localTransforms;
};

struct GTAOSettings
{
    float radius = 0.5f;
    float falloff = 1.0f;
    float intensity = 1.0f;

    uint32_t sampleCount = 8;
};

struct ShadowSettings
{
    uint32_t resolution = 4096;
    uint32_t cascadeCount = 4;
    float splitLambda = 0.85f;
    float maxDistance = 250.0f;
    float shadowBias = 0.0005f;
};

struct RendererResourceSettings
{
    // ============================================================
    // Shadows
    // ============================================================

    uint32_t shadowMapResolution = 4096;
    uint32_t shadowCascadeCount = 4;

    // ============================================================
    // GTAO
    // ============================================================

    uint32_t gtaoResolutionScale = 1;

    // ============================================================
    // Formats
    // ============================================================

    vk::Format depthFormat = vk::Format::eD32Sfloat;
};

struct RuntimeRenderSettings
{
    // ============================================================
    // Visibility
    // ============================================================

    bool enableFrustumCulling = true;
    bool enableOcclusionCulling = true;

    bool enableHiZ = true;
    bool enableLodSelection = true;

    // ============================================================
    // Shadows
    // ============================================================

    bool renderShadows = true;

    // ============================================================
    // Ambient Occlusion
    // ============================================================

    bool renderGtao = true;
    bool renderGtso = true;

    // ============================================================
    // Environment
    // ============================================================

    bool renderSkybox = true;

    // ============================================================
    // Transparency
    // ============================================================

    bool renderTransparency = true;

    // ============================================================
    // Debug
    // ============================================================

    bool renderDebugGeometry = false;
    bool renderDebugLights = false;

    bool visualizeHiZ = false;
    bool visualizeGtao = false;
    bool visualizeCascades = false;

    // ============================================================
    // Screenshot
    // ============================================================

    bool makeScreenshot = false;
};

enum class FrameRecordSegment : uint32_t
{
    Full = 0,
    PreDepth = 1,
    Lighting = 2,
    MainPresent = 3,
};

vk::ResultValue<UploadSceneOutput>
uploadScene(const std::filesystem::path& scenePath, RenderContext& context, vk::Queue transferQueue,
            vk::CommandPool transferCommandPool, vk::DescriptorSetLayout sceneSetLayout,
            vk::ImageView fallbackAlbedoImageView, vk::ImageView fallbackNormalImageView,
            vk::ImageView fallbackOrmImageView, vk::ImageView fallbackEmissionImageView);

vk::ResultValue<DeviceFrameResources> createFrameResources(RenderContext& context,
                                                           const DeviceRendererResources& rendererResources,

                                                           uint32_t renderWidth, uint32_t renderHeight,

                                                           uint32_t drawableCount, uint32_t transformCount,
                                                           uint32_t meshCount,

                                                           uint32_t shadowMapResolution, uint32_t cascadeCount);

vk::ResultValue<DeviceRendererResources> createRendererResources(RenderContext& context, vk::Queue transferQueue,
                                                                 vk::CommandPool transferCommandPool);

vk::Result recordFrameCommands(vk::CommandBuffer cmd, const DeviceRendererResources& renderer,
                               const DeviceSceneResources& scene, const DeviceEnvironmentResources& environment,
                               DeviceFrameResources& frame, const HostSceneData& hostScene,
                               const SceneFrameRequirements& requirements, const RenderTargets& renderTarget,
                               const CascadeSetupPushConstants& cascadePushConstants,
                               const GtaoPushConstants& gtaoPushConstants,
                               const GtaoDenoisePushConstants& gtaoDenoisePushConstants,
                               vk::ImageLayout swapchainOldLayout,
                               const std::function<void(vk::CommandBuffer)>& recordUiCallback);

vk::ResultValue<std::vector<RenderTargets>> createRenderTargets(vk::Device device,
                                                                resources::DeviceAllocator const& allocator,
                                                                std::vector<vk::Image> renderTargetImages,
                                                                vk::Extent2D swapchainExtent,
                                                                vk::Format swapchainFormat);

vk::ResultValue<DeviceEnvironmentResources>
createEnvironmentResources(RenderContext& context, const DeviceRendererResources& rendererResources,
                           vk::Queue transferQueue, vk::CommandPool transferCommandPool,
                           const std::filesystem::path& environmentBlobPath);

} // namespace shuttle::engine::render

#endif // SHUTTLEENGINE_RENDER_HPP
