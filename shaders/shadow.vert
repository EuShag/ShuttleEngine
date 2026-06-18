#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 1, binding = 1) uniform LightData {
    vec4 ambientColor;
    uint lightCount;
    uint pointLightCount;
    uint spottLightCount;
    uint padding;
} sceneLight;

struct DirectionalLightData {
    vec4 direction;
    vec4 color; // rgb = цвет, a = интенсивность
    mat4 lightSpaceMatrix;
};

layout(set = 1, binding = 2) readonly buffer DirectionalLightDatas {
    DirectionalLightData lights[];
} directionalLightDatas;

struct ModelData {
    mat4 modelMatrix;
    mat4 normalMatrix;
};

layout(set = 0, binding = 0) readonly buffer InstanceData {
    ModelData models[];
} instanceData;

void main() {
    mat4 modelMatrix = instanceData.models[gl_InstanceIndex].modelMatrix;
    gl_Position = directionalLightDatas.lights[0].lightSpaceMatrix * modelMatrix * vec4(inPosition, 1.0);
}
