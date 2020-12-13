// VERTEX
// A simple vertex shader to handle FXAA.
#version 400 core
layout (location = 0) in vec3 VtxPosition;
layout (location = 1) in vec2 VtxTexCoord;

out vec2 TexCoords;
out vec4 TexCoordsPosPos;

// some information used by FXAA
uniform vec2 rcpFrame;

void main() {
	gl_Position = vec4(VtxPosition, 1.0f);

	TexCoords = VtxTexCoord;

	TexCoordsPosPos.xy = VtxTexCoord.xy;
	TexCoordsPosPos.zw = VtxTexCoord.xy - (rcpFrame * (0.5 + (1.0/4.0)));
}
