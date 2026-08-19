//
// Created by Shagu on 27.07.2026.
//
#ifndef SHUTTLEENGINE_RENDER_HPP
#define SHUTTLEENGINE_RENDER_HPP
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <Assets/Formats/Lighting.hpp>
#include <Assets/Formats/Scene.hpp>

#include <filesystem>

#include "DeviceAllocator/DeviceAllocator.hpp"
#include "SceneDataLoader/SceneDataLoader.hpp"
#include "DescriptorHeapSet.hpp"

namespace shuttle::engine::render
{
    struct FallbackTextureIndices;

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

    struct Texture {
        resources::UniqueAllocatedImage image;
        vk::UniqueImageView imageView;
        UniqueDescriptorSlot descriptorSlot;
    };

    inline constexpr uint32_t MaxShadowCascades = 4;
    inline constexpr uint32_t MaxHiZMipCount = 16;
    inline constexpr uint32_t MaxBindlessTextures = 2048;
    inline constexpr uint32_t MaxFrustums = 5;

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

    struct RenderRootData {
        vk::DeviceAddress commonDataDeviceAddress{};
        vk::DeviceAddress sceneDataDeviceAddress{};
        vk::DeviceAddress environmentDataDeviceAddress{};
        vk::DeviceAddress cameraDataDeviceAddress{};    

    };

    constexpr vk::DeviceSize PassSpecificDataOffset = sizeof(RenderRootData);

    struct DeviceSceneResources
    {
        resources::UniqueAllocatedBuffer sceneRootBuffer;

        resources::UniqueAllocatedBuffer positionBuffer;
        resources::UniqueAllocatedBuffer attributeBuffer;
        resources::UniqueAllocatedBuffer indexBuffer;
        resources::UniqueAllocatedBuffer meshBuffer;

        resources::UniqueAllocatedBuffer materialBuffer;

        resources::UniqueAllocatedBuffer directionalLightBuffer;

        std::vector<Texture> textures;

        resources::UniqueAllocatedBuffer nodeBuffer;
        resources::UniqueAllocatedBuffer levelBuffer;
        resources::UniqueAllocatedBuffer drawableBuffer;
        resources::UniqueAllocatedBuffer transformBuffer;
    };

    struct DeviceEnvironmentResources
    {
        resources::UniqueAllocatedBuffer environmentBuffer;

        Texture skybox;
        Texture irradiance;
        Texture radiance;
    };

    struct SceneFrameRequirements
    {
        uint32_t transformCount{};
        uint32_t drawableObjectCount{};
        uint32_t meshCount{};
    };

    struct UploadSceneOutput
    {
        HostSceneData hostSceneData;
        DeviceSceneResources deviceSceneResources;
        SceneFrameRequirements sceneFrameRequirements;
    };

    struct ShadowSettings
    {
        uint32_t resolution = 4096;
        uint32_t cascadeCount = 4;
        float splitLambda = 0.85f;
        float maxDistance = 250.0f;
        float shadowBias = 0.0005f;
    };

    enum class FrameRecordSegment : uint32_t
    {
        Full = 0,
        PreDepth = 1,
        Lighting = 2,
        MainPresent = 3,
    };

    vk::ResultValue<UploadSceneOutput> uploadScene(
        const LoadedSceneData& loadedSceneData,
        RenderContext& context,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        DescriptorHeapSet& descriptorHeapSet,
        FallbackTextureIndices const& fallbackTextureIndices);

    vk::ResultValue<DeviceEnvironmentResources> createEnvironmentResources(
        RenderContext& context,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        DescriptorHeapSet& descriptorHeapSet,
        const std::filesystem::path& environmentBlobPath);

} // namespace shuttle::engine::render

#endif // SHUTTLEENGINE_RENDER_HPP
