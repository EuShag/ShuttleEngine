#version 450

#extension GL_EXT_nonuniform_qualifier : require

#include "common_bindings.glsl"
#include "common_scene.glsl"
#include "common_frame.glsl"

// ============================================================
// Inputs
// ============================================================

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec3 inViewPosition;

layout(location = 5) in vec4 inCascadeShadowCoords[SHUTTLE_MAX_SHADOW_CASCADES];

layout(location = 9) flat in uint inMaterialIndex;

// ============================================================
// Output
// ============================================================

layout(location = 0) out vec4 outColor;

// ============================================================
// Renderer Set
// ============================================================

layout(set = SET_RENDERER, binding = RENDERER_MATERIAL_SAMPLER) uniform sampler materialSampler;
layout(set = SET_RENDERER, binding = RENDERER_SHADOW_SAMPLER) uniform sampler shadowSampler;
layout(set = SET_RENDERER, binding = RENDERER_NEAREST_SAMPLER) uniform sampler nearestSampler;
layout(set = SET_RENDERER, binding = RENDERER_BRDF_LUT_IMAGE) uniform texture2D brdfLutImage;

// ============================================================
// Environment Set
// ============================================================

layout(set = SET_ENVIRONMENT, binding = ENV_SKYBOX_IMAGE) uniform textureCube skyboxImage;
layout(set = SET_ENVIRONMENT, binding = ENV_IRRADIANCE_IMAGE) uniform textureCube irradianceImage;
layout(set = SET_ENVIRONMENT, binding = ENV_RADIANCE_IMAGE) uniform textureCube radianceImage;

// ============================================================
// Scene Set
// ============================================================

layout(set = SET_SCENE, binding = SCENE_MATERIALS, std430) readonly buffer MaterialBuffer
{
    MaterialInfo materials[];
};

layout(set = SET_SCENE, binding = SCENE_DIRECTIONAL_LIGHTS, std430) readonly buffer DirectionalLightBuffer
{
    DirectionalLightData directionalLights[];
};

layout(set = SET_SCENE, binding = SCENE_INFO, std140) uniform SceneInfoBuffer
{
    SceneInfo sceneInfo;
};

layout(set = SET_SCENE, binding = SCENE_TEXTURES) uniform texture2D sceneTextures[];

// ============================================================
// Frame Set
// ============================================================

layout(set = SET_FRAME, binding = FRAME_INFO, std140) uniform FrameInfoBuffer
{
    FrameInfo frame;
};

layout(set = SET_FRAME, binding = FRAME_DIRECTIONAL_SHADOW_DATA, std430) readonly buffer DirectionalShadowDataBuffer
{
    DirectionalShadowData shadowData;
};

layout(set = SET_FRAME, binding = FRAME_DIRECTIONAL_SHADOW_MAP_IMAGE) uniform texture2DArray directionalShadowMapImage;

layout(set = SET_FRAME, binding = FRAME_GTAO_FILTERED_IMAGE) uniform texture2D gtaoFilteredImage;
layout(set = SET_FRAME, binding = FRAME_LINEAR_DEPTH_IMAGE) uniform texture2D linearDepthImage;

// ============================================================
// Constants
// ============================================================

const float PI = 3.14159265359;

// ============================================================
// Helpers
// ============================================================

float saturate(float v)
{
    return clamp(v, 0.0, 1.0);
}

bool hasMaterialFlag(uint flags, uint flag)
{
    return (flags & flag) != 0u;
}

vec4 sampleTexture2D(uint textureIndex, vec2 uv, vec4 fallbackValue)
{
    if (textureIndex == INVALID_TEXTURE_U32) return fallbackValue;

    return texture(sampler2D(sceneTextures[nonuniformEXT(textureIndex)], materialSampler), uv);
}

// ============================================================
// PBR
// ============================================================

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{

    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;

    return a2 / max(PI * denom * denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{

    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / max(NdotV * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// ============================================================
// Normal Mapping
// ============================================================

vec3 computeWorldNormal(MaterialInfo material, vec3 geometryNormal, vec4 tangent, vec2 uv)
{

    vec3 N = normalize(geometryNormal);

    if (!hasMaterialFlag(material.flags, MATERIAL_FLAG_HAS_NORMAL_MAP)) return N;
    if (material.normalTexture == INVALID_TEXTURE_U32) return N;
    if (dot(tangent.xyz, tangent.xyz) <= 1e-8 || abs(tangent.w) <= 1e-5) return N;

    vec2 normalXY = sampleTexture2D(material.normalTexture, uv, vec4(0.5, 0.5, 1.0, 1.0)).rg * 2.0 - 1.0;

    normalXY.y = -normalXY.y;

    float normalZ = sqrt(clamp(1.0 - dot(normalXY, normalXY), 0.0, 1.0));

    vec3 tangentNormal = vec3(normalXY, normalZ);
    vec3 T = normalize(tangent.xyz - N * dot(tangent.xyz, N));
    vec3 B = cross(N, T) * tangent.w;
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// ============================================================
// Cascaded Shadows
// ============================================================

uint chooseCascadeIndex(float viewDepth)
{

    uint cascadeIndex = 0u;
    if (viewDepth > shadowData.cascadeSplits.x) cascadeIndex = 1u;
    if (viewDepth > shadowData.cascadeSplits.y) cascadeIndex = 2u;
    if (viewDepth > shadowData.cascadeSplits.z) cascadeIndex = 3u;

    return min(cascadeIndex, uint(SHUTTLE_MAX_SHADOW_CASCADES - 1));
}

float calculateCascadeShadow(uint cascadeIndex, vec4 shadowCoord, vec3 geometryNormal, vec3 lightDirection)
{
    if (shadowCoord.w <= 0.0)
    {
        return 1.0;
    }

    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;

    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 ||
        projCoords.z > 1.0)
    {

        return 1.0;
    }

    ivec2 shadowSize = textureSize(sampler2DArray(directionalShadowMapImage, shadowSampler), 0).xy;
    vec2 texelSize = 1.0 / vec2(shadowSize);

    float NdotL = max(dot(geometryNormal, lightDirection), 0.0);
    float bias = max(0.0006 * (1.0 - NdotL), 0.00008);
    float shadow = 0.0;

    for (int y = -1; y <= 1; ++y)
    {

        for (int x = -1; x <= 1; ++x)
        {

            vec2 offset = vec2(x, y) * texelSize;

            float shadowDepth = texture(sampler2DArray(directionalShadowMapImage, shadowSampler),
                                        vec3(projCoords.xy + offset, float(cascadeIndex)))
                                    .r;

            shadow += (projCoords.z - bias) > shadowDepth ? 0.0 : 1.0;
        }
    }

    return shadow / 9.0;
}

// ============================================================
// GTAO / GTSO
// ============================================================

float sampleGTAO()
{

    vec2 uv = gl_FragCoord.xy * frame.invRenderResolution;

    return texture(sampler2D(gtaoFilteredImage, nearestSampler), uv).r;
}

float computeSpecularOcclusion(float ao, float roughness, float NdotV)
{

    float exponent = mix(8.0, 1.0, roughness);

    float so = pow(saturate(ao), exponent);
    so = mix(so, ao, roughness);

    return saturate(so + 0.15 * (1.0 - roughness) * NdotV);
}

// ============================================================
// Tone Mapping
// ============================================================

vec3 tonemapACES(vec3 color)
{

    vec3 x = color * frame.exposure;

    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// ============================================================
// Main
// ============================================================

void main()
{

    MaterialInfo material = materials[inMaterialIndex];

    // ============================================================
    // Base Color
    // ============================================================

    vec4 baseColorSample = sampleTexture2D(material.albedoTexture, inUv, vec4(1.0));
    vec4 baseColor = baseColorSample * material.baseColorFactor;

    vec3 albedo = pow(baseColor.rgb, vec3(2.2));

    // ============================================================
    // Metallic / Roughness / AO
    // ============================================================

    vec4 ormSample = sampleTexture2D(material.ormTexture, inUv, vec4(1.0, 1.0, 0.0, 1.0));

    float bakedAO = ormSample.r;
    float roughness = ormSample.g * material.roughnessFactor;
    float metallic = ormSample.b * material.metallicFactor;

    roughness = clamp(roughness, 0.04, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    // ============================================================
    // Emissive
    // ============================================================

    vec3 emissiveSample = sampleTexture2D(material.emissiveTexture, inUv, vec4(1.0)).rgb;
    vec3 emissive = emissiveSample * material.emissiveFactor.rgb * material.emissiveStrength;

    // ============================================================
    // Normals / View Vector
    // ============================================================

    vec3 geometryNormal = normalize(inNormal);
    vec3 N = computeWorldNormal(material, geometryNormal, inTangent, inUv);
    vec3 V = normalize(frame.cameraPosition.xyz - inWorldPosition);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ============================================================
    // Direct Directional Light
    // ============================================================

    vec3 Lo = vec3(0.0);

    if (sceneInfo.directionalLightCount > 0u)
    {
        DirectionalLightData light = directionalLights[0];

        // light.directionAndIntensity.xyz is the direction light travels.
        // For shading we need direction from surface to light.
        vec3 L = normalize(-light.directionAndIntensity.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = light.color * light.directionAndIntensity.w;

        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0)
        {
            float viewDepth = -inViewPosition.z;
            uint cascadeIndex = chooseCascadeIndex(viewDepth);

            float shadow = calculateCascadeShadow(
                    cascadeIndex,
                    inCascadeShadowCoords[cascadeIndex],
                    geometryNormal,
                    L);

            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);

            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
            vec3 specular = numerator / denominator;

            Lo += (kD * albedo / PI + specular) *
            radiance *
            NdotL *
            shadow;
        }
    }

    // ============================================================
    // GTAO / GTSO
    // ============================================================

    float gtao = sampleGTAO();
    float finalAO = saturate(bakedAO * gtao);
    float specularOcclusion = computeSpecularOcclusion(finalAO, roughness, NdotV);

    // ============================================================
    // IBL
    // ============================================================

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 irradiance = texture(samplerCube(irradianceImage, materialSampler), N).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    vec3 R = reflect(-V, N);

    const float MAX_REFLECTION_LOD = 5.0;

    vec3 prefilteredColor =
        textureLod(samplerCube(radianceImage, materialSampler), R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(sampler2D(brdfLutImage, materialSampler), vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    vec3 ambient = kD * diffuseIBL * finalAO + specularIBL * specularOcclusion;

    // ============================================================
    // Final Color
    // ============================================================

    vec3 finalColor = ambient + Lo + emissive;
    vec3 mapped = tonemapACES(finalColor);
    vec3 srgb = pow(mapped, vec3(1.0 / frame.gamma));

    outColor = vec4(srgb, baseColor.a);

    vec4 rawNormal =
    texture(
            sampler2D(
                    sceneTextures[nonuniformEXT(material.normalTexture)],
                    materialSampler),
            inUv);

    outColor = vec4(rawNormal.rgb, 1.0);
    return;
    return;
}