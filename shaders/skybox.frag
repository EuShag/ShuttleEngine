#version 450

#include "common_bindings.glsl"

layout(set = SET_RENDERER, binding = RENDERER_MATERIAL_SAMPLER) uniform sampler materialSampler;
layout(set = SET_ENVIRONMENT, binding = ENV_SKYBOX_IMAGE) uniform textureCube skyboxImage;

layout(location = 0) in vec3 inDirection;
layout(location = 0) out vec4 outColor;

void main()
{

    vec3 color = texture(samplerCube(skyboxImage, materialSampler), normalize(inDirection)).rgb;
    outColor = vec4(color, 1.0);
}