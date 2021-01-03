// VERTEX
#version 400 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;

out vec3 WorldPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 model;
uniform mat4 projectionView; // projection * view
uniform mat3 normalMatrix; // transpose(inverse(mat3(model)))

void main() {
	gl_Position = vec4(position.x, position.y, position.z, 1.0);

	Normal = vec3(0, 0, 0);
	TexCoords = vec2(position.x, position.z);
}
