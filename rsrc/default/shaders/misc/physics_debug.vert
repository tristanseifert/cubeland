// VERTEX
#version 400 core
layout (location = 0) in vec3 position;
layout (location = 1) in uint color;

out vec4 VertexColor;

uniform mat4 model;
uniform mat4 projectionView; // projection * view

void main() {
    // unpack vertex color
    VertexColor = vec4((color & 0xFF0000u) >> 16, (color & 0x00FF00u) >> 8, color & 0x0000FFu, 0xFF);

    // Set position of the vertex pls
    vec4 worldPos = model * vec4(position, 1);
    gl_Position = projectionView * worldPos;
}
