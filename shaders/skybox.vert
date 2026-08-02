#version 450

#include "common_bindings.glsl"
#include "common_frame.glsl"

// ============================================================
// Frame Set
// ============================================================

layout(set = SET_FRAME, binding = FRAME_INFO, std140) uniform FrameInfoBuffer
{
    FrameInfo frame;
};

// ============================================================
// Outputs
// ============================================================

layout(location = 0) out vec3 outDirection;

// ============================================================
// Cube Vertices
// ============================================================

const vec3 CubeVertices[36] = {
    // +X
    vec3(1, -1, -1), vec3(1, -1, 1), vec3(1, 1, 1), vec3(1, -1, -1), vec3(1, 1, 1), vec3(1, 1, -1),
    // -X
    vec3(-1, -1, 1), vec3(-1, -1, -1), vec3(-1, 1, -1), vec3(-1, -1, 1), vec3(-1, 1, -1), vec3(-1, 1, 1),
    // +Y
    vec3(-1, 1, -1), vec3(1, 1, -1), vec3(1, 1, 1), vec3(-1, 1, -1), vec3(1, 1, 1), vec3(-1, 1, 1),
    // -Y
    vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, -1, -1), vec3(-1, -1, 1), vec3(1, -1, -1), vec3(-1, -1, -1),
    // +Z
    vec3(-1, -1, 1), vec3(-1, 1, 1), vec3(1, 1, 1), vec3(-1, -1, 1), vec3(1, 1, 1), vec3(1, -1, 1),
    // -Z
    vec3(1, -1, -1), vec3(1, 1, -1), vec3(-1, 1, -1), vec3(1, -1, -1), vec3(-1, 1, -1), vec3(-1, -1, -1)};

// ============================================================
// Main
// ============================================================

void main()
{
    vec3 localPosition = CubeVertices[gl_VertexIndex];
    outDirection = localPosition;

    mat4 viewNoTranslation = mat4(mat3(frame.viewMatrix));
    vec4 clipPosition = frame.projectionMatrix * viewNoTranslation * vec4(localPosition, 1.0);

    gl_Position = clipPosition.xyww;
}