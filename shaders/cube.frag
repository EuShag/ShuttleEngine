#version 460 core

layout (location = 0) in vec3 aDir;

layout (location = 0) out vec4 FragColor;

layout (set = 0, binding = 1) uniform samplerCube uSkybox;

void main(){
	FragColor = texture(uSkybox, normalize(aDir));
}