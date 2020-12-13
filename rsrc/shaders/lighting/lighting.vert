// VERTEX
// A simple vertex shader to handle lighting. It is meant to be rendered onto a
// single quad that fills the entire screen.
#version 400 core
layout (location = 0) in vec3 VtxPosition; // 0..2 in array
layout (location = 1) in vec2 VtxTexCoord; // 3..4 in array

out vec2 TexCoord;

void main() {
	gl_Position = vec4(VtxPosition, 1.0f);
	TexCoord = VtxTexCoord;
}
