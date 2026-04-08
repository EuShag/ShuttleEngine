#version 460 core
// Skybox vertex shader
layout (location = 0) in vec3 aPos;

layout (location = 0) out vec3 TexCoords;

layout (set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 projection;
} ubo;

void main()
{
	TexCoords = aPos;
	mat4 view = mat4(mat3(ubo.view));
	vec4 pos = ubo.projection * view * vec4(aPos, 1.0);
	gl_Position = pos.xyww; // Set w to the depth value to ensure the skybox is rendered at the far plane
}