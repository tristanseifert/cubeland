// FRAGMENT
#version 400 core

layout (location = 0) out vec4 secretion;

in vec2 TexCoords;
uniform sampler2D texture_diffuse1;

void main() {
	// nothing is done since we don't actually render anything (only depth)
    gl_FragDepth = gl_FragCoord.z;

	// vec3 diffuse = texture(texture_diffuse1, TexCoords).rgb;
	// secretion = vec4(diffuse, 1);
}
