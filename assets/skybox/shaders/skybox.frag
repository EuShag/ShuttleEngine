// Fragment shader for skybox
#version 460 core
#extension GL_KHR_vulkan_glsl: enable

layout (location = 0) in vec3 vTexCoords;
layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 1) uniform samplerCube skybox;

void main(){
	FragColor = texture(skybox, vTexCoords);
}