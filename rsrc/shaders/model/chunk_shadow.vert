// VERTEX
#version 400 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;

uniform mat4 model;
uniform mat4 projectionView; // projection * view

void main() {
    // Forward the world position and texture coordinates
    vec4 worldPos = model * vec4(position, 1);

    // Set position of the vertex pls
    gl_Position = projectionView * worldPos;
}
