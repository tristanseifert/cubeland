// VERTEX
#version 400 core
layout (location = 0) in ivec3 position;
layout (location = 1) in uint blockId;
layout (location = 2) in uint faceId;
layout (location = 3) in uint vertexId;

uniform mat4 model;
uniform mat4 projectionView; // projection * view

void main() {
    // Forward the world position and texture coordinates
    vec3 posConverted = vec3(position) / vec3(0x7F);
    vec4 worldPos = model * vec4(posConverted, 1);

    // Set position of the vertex pls
    gl_Position = projectionView * worldPos;
}
