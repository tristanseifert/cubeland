// VERTEX
#version 400 core
layout (location = 0) in vec3 position;
out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
	// this ensures our depth component is 1.0
	vec4 pos = projection * view * vec4(position, 1.0);
	gl_Position = pos.xyww;

	TexCoords = position;
}
