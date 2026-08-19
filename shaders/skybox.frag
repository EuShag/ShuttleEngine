#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common_descriptor_heap.glsl"

#define INVALID_TEXTURE_U32 0xffffffffu

// ============================================================
// Root / Pass Data
// ============================================================

struct RenderRootData
{
    uint64_t commonDataDeviceAddress;
    uint64_t sceneDataDeviceAddress;
    uint64_t environmentDataDeviceAddress;
    uint64_t cameraDataDeviceAddress;
};

struct MainPassData
{
    uint64_t mainPassSettingsAddress;
    uint64_t instanceRemapAddress;

    uint output1Mode;
    uint output2Mode;
    uint output3Mode;
    uint output4Mode;
};

layout(push_constant) uniform PushData
{
    RenderRootData root;
    MainPassData pass;
} pushData;

// ============================================================
// Data
// ============================================================

struct MainPassSettings
{
    vec2 renderResolution;
    vec2 invRenderResolution;

    float exposure;
    float gamma;

    float diffuseIblStrength;
    float specularIblStrength;

    float skyboxIntensity;
    float emissiveIntensity;

    uint reserved0;
    uint reserved1;
};

struct CommonResourcesInfo
{
    uint brdfLutTexture;

    uint materialSampler;
    uint shadowSampler;
    uint nearestSampler;
};

struct EnvironmentGpuInfo
{
    uint skyboxTexture;
    uint irradianceTexture;
    uint radianceTexture;
    uint reserved;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer MainPassSettingsRef
{
    MainPassSettings value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CommonResourcesInfoRef
{
    CommonResourcesInfo value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer EnvironmentGpuInfoRef
{
    EnvironmentGpuInfo value;
};

// ============================================================
// Input / Output
// ============================================================

layout(location = 0) in vec3 inDirection;

layout(location = 0) out vec4 outColor;

#ifdef SHUTTLE_DEBUG_OUTPUTS
layout(location = 1) out vec4 outDebug1;
layout(location = 2) out vec4 outDebug2;
layout(location = 3) out vec4 outDebug3;
layout(location = 4) out vec4 outDebug4;
#endif

// ============================================================
// Helpers
// ============================================================

vec3 tonemapACES(vec3 color, float exposure)
{
    vec3 x = color * exposure;
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 sampleSkybox(
    EnvironmentGpuInfo environment,
    CommonResourcesInfo common_,
    vec3 direction)
{
    if (environment.skyboxTexture == INVALID_TEXTURE_U32) return vec3(0.0);

    return texture(samplerCube(
        heapTextureCube[nonuniformEXT(environment.skyboxTexture)],
        heapSampler[nonuniformEXT(common_.materialSampler)]),
    direction).rgb;
}

// ============================================================
// Main
// ============================================================

void main()
{
    MainPassSettings settings = MainPassSettingsRef(pushData.pass.mainPassSettingsAddress).value;
    CommonResourcesInfo common_ = CommonResourcesInfoRef(pushData.root.commonDataDeviceAddress).value;
    EnvironmentGpuInfo environment = EnvironmentGpuInfoRef(pushData.root.environmentDataDeviceAddress).value;

    vec3 direction = normalize(inDirection);
    vec3 skybox = sampleSkybox(environment, common_, direction) * settings.skyboxIntensity;
    vec3 mapped = tonemapACES(skybox, 1);

    outColor = vec4(mapped, 1.0);

#ifdef SHUTTLE_DEBUG_OUTPUTS
    // Skybox does not provide material/mesh debug data.
    // Write stable useful values so debug attachments are valid.
    outDebug1 = vec4(mapped, 1.0);
    outDebug2 = vec4(direction * 0.5 + 0.5, 1.0);
    outDebug3 = vec4(1.0, 1.0, 1.0, 1.0);
    outDebug4 = vec4(1.0, 1.0, 1.0, 1.0);
#endif
}