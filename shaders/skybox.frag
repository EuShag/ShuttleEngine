#version 460

layout(location = 0) in vec3 vDirection;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0)
uniform sampler linearSampler;

layout(set = 4, binding = 0)
uniform textureCube skyboxMap;

void main()
{
	vec3 color = texture(samplerCube(skyboxMap, linearSampler), normalize(vDirection)).rgb;

	outColor = vec4(color, 1.0);
}