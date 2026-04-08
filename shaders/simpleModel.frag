#version 460 core

layout(location = 0) in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, TexCoords);
}
