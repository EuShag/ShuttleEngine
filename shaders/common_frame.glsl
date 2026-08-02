#ifndef SHUTTLE_COMMON_FRAME_GLSL
#define SHUTTLE_COMMON_FRAME_GLSL

// ============================================================
// Constants
// ============================================================

#define SHUTTLE_MAX_SHADOW_CASCADES 4

// ============================================================
// FrameInfo UBO
// ============================================================

struct FrameInfo
{
    // ============================================================
    // Camera
    // ============================================================

    mat4 viewMatrix;
    mat4 projectionMatrix;

    mat4 viewProjectionMatrix;
    mat4 previousViewProjectionMatrix;

    vec4 cameraPosition;

    // ============================================================
    // Resolution
    // ============================================================

    vec2 renderResolution;
    vec2 invRenderResolution;

    vec2 displayResolution;
    vec2 invDisplayResolution;

    // ============================================================
    // Timing
    // ============================================================

    float deltaTime;
    float elapsedTime;

    uint frameIndex;
    uint drawableCount;

    // ============================================================
    // Camera / Tone Mapping
    // ============================================================

    float nearPlane;
    float farPlane;

    float exposure;
    float gamma;

    uint frustumCount;
    uint shadowCascadeCount;
};

// ============================================================
// Frustum Planes SSBO
// ============================================================
//
// plane.xyz = normal
// plane.w   = distance
//
// dot(plane.xyz, worldPosition) + plane.w

struct FrustumPlanesData
{
    vec4 planes[6];
};

// ============================================================
// Directional Shadow Data SSBO
// ============================================================

struct CascadeShadowData
{
    mat4 lightViewProjection;

    vec4 boundingSphere;
    // xyz = sphere center
    // w   = sphere radius
};

struct DirectionalShadowData
{
    CascadeShadowData cascades[SHUTTLE_MAX_SHADOW_CASCADES];

    vec4 cascadeSplits;
};

// ============================================================
// Render Statistics SSBO
// ============================================================

struct RenderStatistics
{
    uint totalDrawables;

    uint frustumRejected;

    uint firstOcclusionRejected;
    uint secondOcclusionRejected;

    uint candidateDrawables;
    uint visibleCandidates;

    uint visibleDrawables;

    uint totalDrawCalls;
    uint activeDrawCalls;

    uint renderedInstances;
    uint renderedTriangles;

    uint reserved0;
    uint reserved1;
};

#endif