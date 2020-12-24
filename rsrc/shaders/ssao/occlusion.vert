// VERTEX
#version 400 core
layout (location = 0) in vec3 VtxPosition;
layout (location = 1) in vec2 VtxTexCoord;

out vec2 TexCoord;

void main() {
    TexCoord = VtxTexCoord;
    gl_Position = vec4(VtxPosition, 1.0f);
}

