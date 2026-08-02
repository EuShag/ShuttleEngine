#version 450

#extension GL_EXT_multiview : require

#include "common_bindings.glsl"
#include "common_scene.glsl"
#include "common_frame.glsl"

layout(set = SET_SCENE, binding = SCENE_POSITIONS, std430) readonly buffer PositionBuffer
{
    PositionAttribute positions[];
};

layout(set = SET_SCENE, binding = SCENE_MESHES, std430) readonly buffer MeshBuffer
{
    GpuMesh meshes[];
};

layout(set = SET_SCENE, binding = SCENE_DRAWABLES, std430) readonly buffer DrawableBuffer
{
    GpuDrawableObject drawables[];
};

layout(set = SET_FRAME, binding = FRAME_WORLD_TRANSFORMS, std430) readonly buffer WorldTransformBuffer
{
    mat4 worldTransforms[];
};

layout(set = SET_FRAME, binding = FRAME_INSTANCE_REMAP, std430) readonly buffer InstanceRemapBuffer
{
    uint instanceRemap[];
};

layout(set = SET_FRAME, binding = FRAME_DIRECTIONAL_SHADOW_DATA, std430) readonly buffer DirectionalShadowDataBuffer
{
    DirectionalShadowData shadowData;
};

layout(set = SET_FRAME, binding = FRAME_VISIBILITY_MASKS, std430) readonly buffer VisibilityMaskBuffer
{
    uint visibilityMasks[];
};

void main()
{
    uint cascadeIndex = gl_ViewIndex;

    uint drawableId = instanceRemap[gl_InstanceIndex];

    uint visibilityMask = visibilityMasks[drawableId];

    uint cascadeBit = 1u << (cascadeIndex + 1u);

    if ((visibilityMask & cascadeBit) == 0u)
    {
        gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);

        return;
    }

    GpuDrawableObject drawable = drawables[drawableId];

    GpuMesh mesh = meshes[drawable.meshIndex];

    uint localVertexIndex = uint(gl_VertexIndex);

    uint positionIndex = mesh.positionOffset + localVertexIndex;

    vec3 localPosition = positions[positionIndex].position.xyz;

    mat4 modelMatrix = worldTransforms[drawable.transformIndex];

    vec4 worldPosition = modelMatrix * vec4(localPosition, 1.0);

    gl_Position = shadowData.cascades[cascadeIndex].lightViewProjection * worldPosition;
}