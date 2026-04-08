#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aDir;

layout (location = 0) out vec3 Dir;

layout (set = 0, binding = 0) uniform Matrices
{
	mat4 view;
	mat4 projection;
};

void main()
{
	gl_Position = projection * view * vec4(aPos, 1.0);
	Dir = aDir;
}