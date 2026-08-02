//
// Created by Shagu on 30.07.2026.
//

#ifndef SHUTTLEENGINE_SETBINDINGS_HPP
#define SHUTTLEENGINE_SETBINDINGS_HPP
#include "IncludeVulkan.hpp"

namespace shuttle::engine::render
{

enum class DescriptorSets : uint32_t
{
    eRenderer = 0,
    eEnvironment = 1,
    eScene = 2,
    eFrame = 3
};

enum class RendererBindings : uint32_t
{
    eMaterialSampler = 0,
    eShadowSampler = 1,
    eNearestSampler = 2,
    eBrdfLutImage = 3
};

enum class EnvironmentBindings : uint32_t
{
    eSkyboxImage = 0,
    eIrradianceImage = 1,
    eRadianceImage = 2
};

enum class SceneBindings : uint32_t
{
    eNodes = 0,
    eNodeLevels = 1,
    eTransforms = 2,

    eDrawables = 3,

    ePositions = 4,
    eAttributes = 5,
    eMeshes = 6,
    eIndices = 7,

    eMaterials = 8,
    eDirectionalLights = 9,

    eSceneInfo = 10,
    eTextures = 11
};

enum class FrameBindings : uint32_t
{
    // ============================================================
    // Camera
    // ============================================================

    eFrameInfo = 0,
    eFrustumPlanes = 1,

    // ============================================================
    // Directional Shadows
    // ============================================================

    eDirectionalShadowData = 2,

    // ============================================================
    // Scene Update
    // ============================================================

    eWorldTransforms = 3,

    // ============================================================
    // Frustum Culling
    // ============================================================

    eCandidateIndices = 4,
    eCandidateCount = 5,

    // ============================================================
    // Hi-Z Occlusion Culling
    // ============================================================

    eVisibilityFlags = 6,
    eChosenMeshIds = 7,

    // ============================================================
    // Draw Command Generation
    // ============================================================

    eIndirectCommands = 8,
    eDrawCount = 9,

    eMeshRanges = 10,

    // ============================================================
    // Instance Resolve
    // ============================================================

    eMeshWriteCounters = 11,
    eInstanceRemap = 12,

    // ============================================================
    // Sampled Images
    // ============================================================

    eDepthImage = 13,
    eLinearDepthImage = 14,
    eHiZPyramidImage = 15,

    eGtaoImage = 16,
    eGtaoFilteredImage = 17,

    eDirectionalShadowMapImage = 18,

    // ============================================================
    // Statistics
    // ============================================================

    eRenderStatistics = 19,

    // ============================================================
    // Storage Images
    // ============================================================

    eLinearDepthStorageImage = 20,

    eHiZStorageImages = 21,

    eGtaoStorageImage = 22,
    eGtaoFilteredStorageImage = 23,

    // ============================================================
    // Hi-Z Atomic Cascade
    // ============================================================

    eHiZCounters = 24,

    // ============================================================
    // Visibility Masks
    // ============================================================

    eVisibilityMasks = 25,

    // ============================================================
    // Occlusion Pass #1
    // ============================================================

    eVisibleCandidateIndices = 26,
    eVisibleCandidateCount = 27
};

} // namespace shuttle::engine::render

#endif // SHUTTLEENGINE_SETBINDINGS_HPP
