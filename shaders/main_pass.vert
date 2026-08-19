#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// ============================================================
// Constants
// ============================================================

#define INVALID_INDEX_U32 0xffffffffu

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
    uint64_t worldTransformBufferAddress;

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
// Camera / Scene Data
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

struct DrawableGpuInfo
{
    uint meshIndex;
    uint materialIndex;
    uint transformIndex;
    uint reserved;
};

struct MeshLodGpuInfo
{
    uint firstIndex;
    uint indexCount;

    float geometricError;
    float screenThreshold;
};

struct MeshGpuInfo
{
    uint64_t positionAttributeBufferAddress;
    uint64_t normalUvTangentAttributeBufferAddress;

    vec4 boundingSphere;
    vec4 minAABB;
    vec4 maxAABB;


    uint meshFlags;
    uint lodCount;
    uint64_t reserved0;
    MeshLodGpuInfo lods[4];
};

struct PositionAttribute
{
    vec4 position;
};

struct VertexAttribute
{
    vec4 normal;
    vec4 tangent;
    vec4 uv;
};

// ============================================================
// Buffer References
// ============================================================

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer CameraDataRef
{
    CameraData value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer SceneGpuInfoRef
{
    SceneGpuInfo value;
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer DrawableBufferRef
{
    DrawableGpuInfo values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer MeshBufferRef
{
    MeshGpuInfo values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer PositionBufferRef
{
    PositionAttribute values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer VertexAttributeBufferRef
{
    VertexAttribute values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16)
readonly buffer WorldTransformBufferRef
{
    mat4 values[];
};

layout(buffer_reference, std430, buffer_reference_align = 4)
readonly buffer UintBufferRef
{
    uint values[];
};

// ============================================================
// Outputs
// ============================================================

layout(location = 0) out vec3 outLocalPosition;
layout(location = 1) out vec3 outWorldPosition;

layout(location = 2) out vec3 outVertexNormal;
layout(location = 3) out vec3 outWorldNormal;

layout(location = 4) out vec4 outVertexTangent;
layout(location = 5) out vec4 outWorldTangent;

layout(location = 6) out vec2 outUv;
layout(location = 7) out vec3 outViewPosition;

layout(location = 8) flat out uint outMeshIndex;
layout(location = 9) flat out uint outMaterialIndex;
layout(location = 10) flat out uint outDrawableIndex;
layout(location = 11) flat out uint outInstanceIndex;

// ============================================================
// Main
// ============================================================

void main()
{
    CameraData camera = CameraDataRef(pushData.root.cameraDataDeviceAddress).value;
    SceneGpuInfo scene = SceneGpuInfoRef(pushData.root.sceneDataDeviceAddress).value;
    DrawableBufferRef drawables = DrawableBufferRef(scene.drawablesBufferAddress);
    MeshBufferRef meshes = MeshBufferRef(scene.meshDatasBufferAddress);
    UintBufferRef instanceRemap = UintBufferRef(pushData.pass.instanceRemapAddress);
    WorldTransformBufferRef worldTransforms = WorldTransformBufferRef(pushData.pass.worldTransformBufferAddress);

    uint remapIndex = gl_InstanceIndex;
    uint drawableIndex = instanceRemap.values[remapIndex];
    
    DrawableGpuInfo drawable = drawables.values[drawableIndex];
    MeshGpuInfo mesh = meshes.values[drawable.meshIndex];
    PositionBufferRef positions = PositionBufferRef(mesh.positionAttributeBufferAddress);
    VertexAttributeBufferRef attributes = VertexAttributeBufferRef(mesh.normalUvTangentAttributeBufferAddress);

    uint localVertexIndex = uint(gl_VertexIndex);

    PositionAttribute positionAttribute = positions.values[localVertexIndex];

    VertexAttribute vertexAttribute = attributes.values[localVertexIndex];

    vec3 localPosition = positionAttribute.position.xyz;
    vec3 localNormal = normalize(vertexAttribute.normal.xyz);
    vec4 localTangent = vertexAttribute.tangent;
    vec2 uv = vertexAttribute.uv.xy;
    mat4 modelMatrix = worldTransforms.values[drawable.transformIndex];
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    vec4 worldPosition = modelMatrix * vec4(localPosition, 1.0);
    vec3 worldNormal = normalize(normalMatrix * localNormal);
    vec3 worldTangent = normalize(normalMatrix * localTangent.xyz);
    vec4 viewPosition = camera.viewMatrix * worldPosition;

    outLocalPosition = localPosition;
    outWorldPosition = worldPosition.xyz;
    outVertexNormal = localNormal;
    outWorldNormal = worldNormal;
    outVertexTangent = localTangent;
    outWorldTangent = vec4(worldTangent, localTangent.w);
    outUv = uv;
    outViewPosition = viewPosition.xyz;
    outMeshIndex = drawable.meshIndex;
    outMaterialIndex = drawable.materialIndex;
    outDrawableIndex = drawableIndex;
    outInstanceIndex = uint(gl_InstanceIndex);

    gl_Position = camera.viewProjectionMatrix * worldPosition;
}