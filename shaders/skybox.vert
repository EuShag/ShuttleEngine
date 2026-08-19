#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

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
// Camera
// ============================================================

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

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CameraDataRef
{
    CameraData value;
};

// ============================================================
// Output
// ============================================================

layout(location = 0) out vec3 outDirection;

// ============================================================
// Cube
// ============================================================

vec3 getCubeVertex(uint vertexIndex)
{
    const vec3 vertices[36] = {
        vec3(-1.0, -1.0, -1.0),
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, -1.0, -1.0),

        vec3(-1.0, -1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(-1.0, -1.0, 1.0),
        vec3(-1.0, 1.0, 1.0),

        vec3(-1.0, 1.0, 1.0),
        vec3(-1.0, -1.0, 1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, 1.0, 1.0),

        vec3(1.0, 1.0, 1.0),
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 1.0, -1.0),

        vec3(-1.0, -1.0, -1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3(-1.0, -1.0, 1.0),

        vec3(-1.0, 1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(-1.0, 1.0, 1.0),
        vec3(-1.0, 1.0, -1.0)
    };

    return vertices[vertexIndex];
}

// ============================================================
// Main
// ============================================================

void main()
{
    CameraData camera = CameraDataRef(pushData.root.cameraDataDeviceAddress).value;

    vec3 position = getCubeVertex(uint(gl_VertexIndex));
    mat4 viewNoTranslation = camera.viewMatrix;
    viewNoTranslation[3] = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 clipPosition = camera.projectionMatrix * viewNoTranslation * vec4(position, 1.0);

    outDirection = position;
    gl_Position = clipPosition.xyww;
}