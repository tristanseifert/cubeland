// FRAGMENT
#version 400 core
in vec3 TexCoords;

uniform samplerCube skyboxTex;

// output fragment color
out vec4 FragColour;

void main() {
	// FragColour = vec4(TexCoords, 1);
	FragColour = texture(skyboxTex, TexCoords);
}
