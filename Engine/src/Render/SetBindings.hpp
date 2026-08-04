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
        // Frame
        // ============================================================

        eFrameInfo = 0,

        // ============================================================
        // Shadows
        // ============================================================

        eDirectionalShadowData = 2,

        // ============================================================
        // Scene Update
        // ============================================================

        eWorldTransforms = 3,

        eMeshRanges = 4,
        eInstanceRemap = 5,

        // ============================================================
        // Draw Generation
        // ============================================================

        eIndirectCommands = 6,
        eDrawCount = 7,
    };

} // namespace shuttle::engine::render

#endif // SHUTTLEENGINE_SETBINDINGS_HPP
