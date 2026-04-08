#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aTexPos;

layout(location = 0) out vec2 vTexPos;

layout(set = 0, binding = 1) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(aPos, 1.0);
    vTexPos = aTexPos.xy;
}
