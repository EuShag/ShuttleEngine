#version 450

#include "common_bindings.glsl"
#include "common_scene.glsl"
#include "common_frame.glsl"

// ============================================================
// Scene Set
// ============================================================

layout(set = SET_SCENE, binding = SCENE_POSITIONS, std430) readonly buffer PositionBuffer
{
    PositionAttribute positions[];
};

layout(set = SET_SCENE, binding = SCENE_ATTRIBUTES, std430) readonly buffer AttributeBuffer
{
    VertexAttribute attributes[];
};

layout(set = SET_SCENE, binding = SCENE_DRAWABLES, std430) readonly buffer DrawableBuffer
{
    GpuDrawableObject drawables[];
};

layout(set = SET_SCENE, binding = SCENE_MESHES, std430) readonly buffer MeshBuffer
{
    GpuMesh meshes[];
};

// ============================================================
// Frame Set
// ============================================================

layout(set = SET_FRAME, binding = FRAME_INFO, std140) uniform FrameInfoBuffer
{
    FrameInfo frame;
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

// ============================================================
// Outputs
// ============================================================

layout(location = 0) out vec3 outWorldPosition;

layout(location = 1) out vec3 outNormal;

layout(location = 2) out vec2 outUv;

layout(location = 3) out vec4 outTangent;

layout(location = 4) out vec3 outViewPosition;

layout(location = 5) out vec4 outCascadeShadowCoords[SHUTTLE_MAX_SHADOW_CASCADES];

layout(location = 9) flat out uint outMaterialIndex;

// ============================================================
// Main
// ============================================================

void main()
{

    uint drawableId = instanceRemap[gl_InstanceIndex];

    GpuDrawableObject drawable = drawables[drawableId];

    GpuMesh mesh = meshes[drawable.meshIndex];

    uint localVertexIndex = uint(gl_VertexIndex);
    uint positionIndex = mesh.positionOffset + localVertexIndex;
    uint attributeIndex = mesh.attributeOffset + localVertexIndex;

    PositionAttribute positionAttribute = positions[positionIndex];

    VertexAttribute vertexAttribute = attributes[attributeIndex];

    vec3 localPosition = positionAttribute.position.xyz;
    vec3 localNormal = normalize(vertexAttribute.normal.xyz);
    vec4 localTangent = vertexAttribute.tangent;
    vec2 localUv = vertexAttribute.uv.xy;

    mat4 modelMatrix = worldTransforms[drawable.transformIndex];
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    vec4 worldPosition = modelMatrix * vec4(localPosition, 1.0);

    outWorldPosition = worldPosition.xyz;
    outNormal = normalize(normalMatrix * localNormal);
    outUv = localUv;
    outTangent = vec4(normalize(normalMatrix * localTangent.xyz), localTangent.w);

    vec4 viewPosition = frame.viewMatrix * worldPosition;

    outViewPosition = viewPosition.xyz;

    for (uint cascadeIndex = 0u; cascadeIndex < SHUTTLE_MAX_SHADOW_CASCADES; ++cascadeIndex) {
        outCascadeShadowCoords[cascadeIndex] = shadowData.cascades[cascadeIndex].lightViewProjection * worldPosition;
    }

    outMaterialIndex = drawable.materialIndex;

    gl_Position = frame.viewProjectionMatrix * worldPosition;
}