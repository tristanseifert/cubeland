// VERTEX
#version 400 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 projectionView; // projection * view
uniform mat3 normalMatrix; // transpose(inverse(mat3(model)))

void main() {
	// Forward the world position and texture coordinates
	vec4 worldPos = model * vec4(position, 1.0f);
	TexCoords = texCoords;

	// Set position of the vertex pls
	gl_Position = projectionView * worldPos;
}
