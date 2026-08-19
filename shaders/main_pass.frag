#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common_descriptor_heap.glsl"

// ============================================================
// Debug Modes
// ============================================================

#define DEBUG_FINAL              0u
#define DEBUG_ALBEDO             1u
#define DEBUG_NORMAL             2u
#define DEBUG_TANGENT            3u
#define DEBUG_BITANGENT          4u
#define DEBUG_METALLIC           5u
#define DEBUG_ROUGHNESS          6u
#define DEBUG_AO                 7u
#define DEBUG_EMISSIVE           8u
#define DEBUG_UV                 9u
#define DEBUG_MESH_ID            10u
#define DEBUG_MATERIAL_ID        11u
#define DEBUG_INSTANCE_ID        12u
#define DEBUG_VIEW_DEPTH         13u
#define DEBUG_LINEAR_DEPTH       14u
#define DEBUG_WORLD_POSITION     15u
#define DEBUG_WORLD_NORMAL       16u

#define INVALID_TEXTURE_U32      0xffffffffu
#define MATERIAL_FLAG_HAS_NORMAL_MAP 1u

// ============================================================
// Constants
// ============================================================

const float PI = 3.14159265359;
const float INV_PI = 1.0 / PI;

// ============================================================
// Render Root / Pass Data
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
};

layout(push_constant) uniform PushData
{
    RenderRootData root;
    MainPassData pass;
} pushData;

// ============================================================
// GPU Data
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

    uint output1Mode;
    uint output2Mode;
    uint output3Mode;
    uint output4Mode;

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

struct CameraData
{
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;

    mat4 inverseViewMatrix;
    mat4 inverseProjectionMatrix;
    mat4 inverseViewProjectionMatrix;

    vec4 cameraPosition;

    float nearPlane;
    float farPlane;
    float fov;
    float aspectRatio;
};

struct SceneGpuInfo
{
    uint64_t sceneNodesBufferDeviceAddress;
    uint64_t materialDatasBufferAddress;
    uint64_t lightDatasBufferAddress;
    uint64_t meshDatasBufferAddress;
    uint64_t drawablesBufferAddress;
    uint64_t localTransformsBufferAddress;

    uint materialCount;
    uint lightCount;
    uint meshCount;
    uint drawableCount;
    uint nodeCount;
    uint padding0;
};

struct EnvironmentGpuInfo
{
    uint skyboxTexture;
    uint irradianceTexture;
    uint radianceTexture;
    uint radianceMaxLod;
};

struct MaterialGpuInfo
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;

    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    float occlusionStrength;

    float emissiveStrength;

    uint albedoTexture;
    uint normalTexture;
    uint ormTexture;
    uint emissiveTexture;

    uint flags;
    uint pipelineFlags;
    uint alphaMode;
};

struct DirectionalLightGpuInfo
{
    vec4 lightDirection; // xyz = direction in which light travels.
    vec4 lightColorAndIntensity; // rgb = color, a = intensity
};

// ============================================================
// Buffer References
// ============================================================

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer MainPassSettingsRef { MainPassSettings value; };
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer CommonResourcesInfoRef { CommonResourcesInfo value; };
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer CameraDataRef { CameraData value; };
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer SceneGpuInfoRef { SceneGpuInfo value; };
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer EnvironmentGpuInfoRef { EnvironmentGpuInfo value; };
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer MaterialBufferRef { MaterialGpuInfo values[]; };
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer DirectionalLightBufferRef { DirectionalLightGpuInfo values[]; };

// ============================================================
// Inputs
// ============================================================

layout(location = 0) in vec3 inLocalPosition;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) in vec3 inVertexNormal;
layout(location = 3) in vec3 inWorldNormal;
layout(location = 4) in vec4 inVertexTangent;
layout(location = 5) in vec4 inWorldTangent;
layout(location = 6) in vec2 inUv;
layout(location = 7) in vec3 inViewPosition;
layout(location = 8) flat in uint inMeshIndex;
layout(location = 9) flat in uint inMaterialIndex;
layout(location = 10) flat in uint inDrawableIndex;
layout(location = 11) flat in uint inInstanceIndex;

// ============================================================
// Outputs
// ============================================================

layout(location = 0) out vec4 outColor;

#ifdef SHUTTLE_DEBUG_OUTPUTS
layout(location = 1) out vec4 outDebug1;
layout(location = 2) out vec4 outDebug2;
layout(location = 3) out vec4 outDebug3;
layout(location = 4) out vec4 outDebug4;
#endif

// ============================================================
// Basic Helpers
// ============================================================

float saturate(float value) { return clamp(value, 0.0, 1.0); }
bool hasFlag(uint flags, uint flag) { return (flags & flag) != 0u; }
vec3 idToColor(uint id) { return vec3(fract(float(id) * 0.1031), fract(float(id) * 0.11369), fract(float(id) * 0.13787)); }

// ============================================================
// Texture Helpers
// ============================================================

vec4 sampleTexture2D(uint textureIndex, uint samplerIndex, vec2 uv, vec4 fallback)
{
    return texture(sampler2D(heapTexture2D[nonuniformEXT(textureIndex)], heapSampler[nonuniformEXT(samplerIndex)]), uv);
}

vec3 sampleTextureCube(uint textureIndex, uint samplerIndex, vec3 direction, vec3 fallback)
{
    return texture(samplerCube(heapTextureCube[nonuniformEXT(textureIndex)], heapSampler[nonuniformEXT(samplerIndex)]), direction).rgb;
}

vec3 sampleTextureCubeLod(uint textureIndex, uint samplerIndex, vec3 direction, float lod, vec3 fallback)
{
    return textureLod(samplerCube(heapTextureCube[nonuniformEXT(textureIndex)], heapSampler[nonuniformEXT(samplerIndex)]), direction, lod).rgb;
}

// ============================================================
// Normal Mapping
// ============================================================

// Измененная функция: теперь она возвращает также модификатор шероховатости
struct MappedNormalResult
{
    vec3 normal;
    float roughnessModifier; // Новый модификатор
};

MappedNormalResult computeMappedNormal(
        MaterialGpuInfo material,
        CommonResourcesInfo commonResources,
        vec3 geometryNormal,
        vec4 worldTangent,
        vec2 uv)
{
    vec3 N = normalize(geometryNormal);
    float roughnessModifier = 0.0; // По умолчанию нет модификации

    if (dot(worldTangent.xyz, worldTangent.xyz) <= 1e-8 || abs(worldTangent.w) <= 1e-5)
    {
        return MappedNormalResult(N, roughnessModifier);
    }

    vec2 rawNormalXY = sampleTexture2D(
            material.normalTexture,
            commonResources.materialSampler,
            uv,
            vec4(0.5, 0.5, 1.0, 1.0)).rg * 2.0 - 1.0;

    float rawLengthSq = dot(rawNormalXY, rawNormalXY);
    roughnessModifier = saturate(1.0 - sqrt(rawLengthSq)); // Расчет модификатора!

    vec2 normalXY = rawNormalXY;
    normalXY.y = -normalXY.y;

    float normalZ = sqrt(clamp(1.0 - rawLengthSq, 0.0, 1.0));
    vec3 tangentNormal = vec3(normalXY, normalZ);

    vec3 T = normalize(worldTangent.xyz - N * dot(worldTangent.xyz, N));
    vec3 B = cross(N, T) * worldTangent.w;
    mat3 TBN = mat3(T, B, N);

    return MappedNormalResult(normalize(TBN * tangentNormal), roughnessModifier);
}

// ============================================================
// PBR Helpers
// ============================================================

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(float NoH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-5);
}

float geometrySchlickGGX(float NoV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NoV / max(NoV * (1.0 - k) + k, 1e-5);
}

float geometrySmith(float NoV, float NoL, float roughness)
{
    float ggxV = geometrySchlickGGX(NoV, roughness);
    float ggxL = geometrySchlickGGX(NoL, roughness);
    return ggxV * ggxL;
}

vec3 evaluateDirectionalLight(
        vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0,
        DirectionalLightGpuInfo light)
{
    vec3 L = normalize(light.lightDirection.xyz);
    vec3 H = normalize(V + L);
    vec3 lightRadiance = light.lightColorAndIntensity.rgb * light.lightColorAndIntensity.a;

    float NoV = max(dot(N, V), 0.0);
    float NoL = max(dot(N, L), 0.0);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    if (NoL <= 0.0 || NoV <= 0.0) return vec3(0.0);

    float D = distributionGGX(NoH, roughness);
    float G = geometrySmith(NoV, NoL, roughness);
    vec3 F = fresnelSchlick(VoH, F0); // Френель для прямого света

    vec3 numerator = D * G * F;
    float denominator = max(4.0 * NoV * NoL, 1e-5);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo * INV_PI;

    return (diffuse + specular) * lightRadiance * NoL;
}

vec3 tonemapACES(vec3 color, float exposure)
{
    vec3 x = color * exposure;
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// ============================================================
// Debug
// ============================================================

vec4 evaluateDebugOutput(
        uint mode, vec3 finalColor, vec3 albedo, vec3 mappedNormal,
        vec3 worldNormal, vec3 worldTangent, vec3 bitangent, vec4 orm,
        vec3 emissive, float viewDepth, float linearDepth)
{
    if (mode == DEBUG_FINAL) return vec4(finalColor, 1.0);
    if (mode == DEBUG_ALBEDO) return vec4(albedo, 1.0);
    if (mode == DEBUG_NORMAL) return vec4(mappedNormal * 0.5 + 0.5, 1.0);
    if (mode == DEBUG_TANGENT) return vec4(normalize(worldTangent) * 0.5 + 0.5, 1.0);
    if (mode == DEBUG_BITANGENT) return vec4(normalize(bitangent) * 0.5 + 0.5, 1.0);
    if (mode == DEBUG_METALLIC) return vec4(vec3(orm.b), 1.0);
    if (mode == DEBUG_ROUGHNESS) return vec4(vec3(orm.g), 1.0);
    if (mode == DEBUG_AO) return vec4(vec3(orm.r), 1.0);
    if (mode == DEBUG_EMISSIVE) return vec4(emissive, 1.0);
    if (mode == DEBUG_UV) return vec4(fract(inUv), 0.0, 1.0);
    if (mode == DEBUG_MESH_ID) return vec4(idToColor(inMeshIndex), 1.0);
    if (mode == DEBUG_MATERIAL_ID) return vec4(idToColor(inMaterialIndex), 1.0);
    if (mode == DEBUG_INSTANCE_ID) return vec4(idToColor(inInstanceIndex), 1.0);
    if (mode == DEBUG_VIEW_DEPTH) return vec4(vec3(viewDepth), 1.0);
    if (mode == DEBUG_LINEAR_DEPTH) return vec4(vec3(linearDepth), 1.0);
    if (mode == DEBUG_WORLD_POSITION) return vec4(inWorldPosition, 1.0);
    if (mode == DEBUG_WORLD_NORMAL) return vec4(worldNormal * 0.5 + 0.5, 1.0);
    return vec4(0.0, 0.0, 0.0, 1.0);
}

// ============================================================
// Main
// ============================================================

void main()
{
    MainPassSettings settings = MainPassSettingsRef(pushData.pass.mainPassSettingsAddress).value;
    CommonResourcesInfo commonResources = CommonResourcesInfoRef(pushData.root.commonDataDeviceAddress).value;
    CameraData camera = CameraDataRef(pushData.root.cameraDataDeviceAddress).value;
    SceneGpuInfo scene = SceneGpuInfoRef(pushData.root.sceneDataDeviceAddress).value;
    EnvironmentGpuInfo environment = EnvironmentGpuInfoRef(pushData.root.environmentDataDeviceAddress).value;
    MaterialBufferRef materials = MaterialBufferRef(scene.materialDatasBufferAddress);
    DirectionalLightBufferRef directionalLights = DirectionalLightBufferRef(scene.lightDatasBufferAddress);
    MaterialGpuInfo material = materials.values[inMaterialIndex];

    // ============================================================
    // Material sampling
    // ============================================================

    vec4 albedoSample = sampleTexture2D(material.albedoTexture, commonResources.materialSampler, inUv, vec4(1.0));
    vec4 ormSample = sampleTexture2D(material.ormTexture, commonResources.materialSampler, inUv, vec4(1.0, 1.0, 0.0, 1.0));
    vec4 emissiveSample = sampleTexture2D(material.emissiveTexture, commonResources.materialSampler, inUv, vec4(0.0));
    vec4 baseColor = albedoSample * material.baseColorFactor;

    if (baseColor.a < material.alphaCutoff) discard;

    vec3 albedo = baseColor.rgb;

    float ao = 1.0; // Temporarily forced to 1.0. Use ormSample.r for actual AO.
    float baseRoughness = clamp(ormSample.g * material.roughnessFactor, 0.04, 1.0); // Базовая шероховатость
    float metallic = clamp(ormSample.b * material.metallicFactor, 0.0, 1.0);
    vec3 emissive = emissiveSample.rgb * material.emissiveFactor.rgb * material.emissiveStrength * settings.emissiveIntensity;

    // ============================================================
    // Basis & Normals (with roughness modification)
    // ============================================================

    vec3 worldNormal = normalize(inWorldNormal);
    MappedNormalResult normalResult = computeMappedNormal(
            material,
            commonResources,
            worldNormal,
            inWorldTangent,
            inUv);

    vec3 N = normalResult.normal;
    // МОДИФИЦИРУЕМ ROUGHNESS НА ОСНОВЕ ФАКТОРА НОРМАЛИ!
    float roughness = clamp(baseRoughness + normalResult.roughnessModifier * 0.5, 0.04, 1.0);

    vec3 V = normalize(camera.cameraPosition.xyz - inWorldPosition);
    float NoV = max(dot(N, V), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ============================================================
    // Direct lighting
    // ============================================================

    vec3 directLighting = vec3(0.0);
    for (uint lightIndex = 0u;lightIndex < scene.lightCount;++lightIndex)
    {
        DirectionalLightGpuInfo light = directionalLights.values[lightIndex];
        directLighting += evaluateDirectionalLight(N, V, albedo, metallic, roughness, F0, light);
    }

    // ============================================================
    // IBL diffuse
    // ============================================================

    vec3 F = fresnelSchlickRoughness(NoV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = sampleTextureCube(
            environment.irradianceTexture,
            commonResources.materialSampler,
            N,
            vec3(0.0)) * settings.diffuseIblStrength;

    // ИСПРАВЛЕННЫЙ РАСЧЕТ: kD применяется к albedo * irradiance
    vec3 diffuseIBL = kD * albedo * irradiance * ao; // ao здесь выступает как множитель яркости

    // ============================================================
    // IBL specular
    // ============================================================

    vec3 R = reflect(-V, N);

    // MAX_REFLECTION_LOD теперь берется из структуры окружения (CPU)
    float prefilteredLod = roughness * environment.radianceMaxLod;

    vec3 prefilteredColor = sampleTextureCubeLod(
            environment.radianceTexture,
            commonResources.materialSampler,
            R,
            prefilteredLod,
            vec3(0.0)) * settings.specularIblStrength;

    vec2 brdf = sampleTexture2D(
            commonResources.brdfLutTexture,
            commonResources.materialSampler,
            vec2(NoV, roughness),
            vec4(0.0)).rg;

    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 ambient = diffuseIBL + specularIBL;

    // ============================================================
    // Final
    // ============================================================

    // Добавляем минимальный базовый свет, чтобы тени не были абсолютно черными
    // Этот свет не зависит от IBL, а просто слегка приподнимает тени.
    vec3 minAmbientLift = vec3(0.01) * albedo; // 1% от альбедо всегда светится

    vec3 finalColor = ambient + directLighting + emissive;

    // Ваш tonemapping и gamma
    vec3 mapped = tonemapACES(finalColor, settings.exposure);
    vec3 srgb = pow(mapped, vec3(1.0 / settings.gamma));

    outColor = vec4(srgb, 1.0);

    #ifdef SHUTTLE_DEBUG_OUTPUTS
    vec3 bitangent = cross(normalize(inWorldNormal), normalize(inWorldTangent.xyz)) * inWorldTangent.w;
    float viewDepth = -inViewPosition.z;
    float linearDepth = (viewDepth - camera.nearPlane) / max(camera.farPlane - camera.nearPlane, 0.0001);
    vec4 orm = vec4(ao, roughness, metallic, 1.0);

    outDebug1 = evaluateDebugOutput(settings.output1Mode, srgb, albedo, N, worldNormal, inWorldTangent.xyz, bitangent, orm, emissive, viewDepth, linearDepth);
    outDebug2 = evaluateDebugOutput(settings.output2Mode, srgb, albedo, N, worldNormal, inWorldTangent.xyz, bitangent, orm, emissive, viewDepth, linearDepth);
    outDebug3 = evaluateDebugOutput(settings.output3Mode, srgb, albedo, N, worldNormal, inWorldTangent.xyz, bitangent, orm, emissive, viewDepth, linearDepth);
    outDebug4 = evaluateDebugOutput(settings.output4Mode, srgb, albedo, N, worldNormal, inWorldTangent.xyz, bitangent, orm, emissive, viewDepth, linearDepth);
    #endif
}