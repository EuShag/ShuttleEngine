#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;

layout(set = 2, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec3 cameraPos;
} camera;

struct ModelData {
    mat4 modelMatrix;
    mat4 normalMatrix;
};

layout(set = 1, binding = 0) readonly buffer InstanceData {
    ModelData models[];
} instanceData;

struct DirectionalLightData {
    vec4 direction;
    vec4 color; // rgb = цвет, a = интенсивность
    mat4 lightSpaceMatrix;
};

layout(set = 2, binding = 1) uniform LightData {
    vec4 ambientColor;
    uint lightCount;
    uint pointLightCount;
    uint spottLightCount;
    uint padding;
} sceneLight;

layout(set = 2, binding = 2) readonly buffer DirectionalLightDatas {
    DirectionalLightData lights[];
} directionalLightDatas;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) out vec4 outTangent;
layout(location = 4) out vec4 outShadowCoord;

void main() {
    mat4 modelMatrix = instanceData.models[gl_InstanceIndex].modelMatrix;
    mat3 normalMatrix = mat3(instanceData.models[gl_InstanceIndex].normalMatrix);

    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);

    outWorldPos = worldPos.xyz;
    outNormal = normalize(normalMatrix * inNormal);
    outUv = inUv;
    outTangent = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);

    // Рассчитываем координаты в пространстве света (используем первый источник - Солнце)
    if (sceneLight.lightCount > 0) {
        outShadowCoord = directionalLightDatas.lights[0].lightSpaceMatrix * worldPos;
    } else {
        outShadowCoord = vec4(0.0);
    }

    gl_Position = camera.viewProj * worldPos;
}
