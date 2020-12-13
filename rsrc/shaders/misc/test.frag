// FRAGMENT
#version 400 core

// Material data
struct MaterialStruct {
	// how reflective the material is: lower value = more reflective
	float shininess;
};

// The normal buffer is colour attachment 0
layout (location = 0) out vec3 gNormal;
// The diffuse buffer is colour attachment 1
layout (location = 1) out vec3 gDiffuse;
// The material property buffer is colour attachment 2
layout (location = 2) out vec4 gMatProps;

// Inputs from vertex shader
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

// Number of textures to sample (diffuse, specular)
uniform vec2 NumTextures;

// Samplers (for diffuse and specular)
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform sampler2D texture_diffuse3;
uniform sampler2D texture_diffuse4;

uniform sampler2D texture_specular1;
uniform sampler2D texture_specular2;
uniform sampler2D texture_specular3;
uniform sampler2D texture_specular4;

// Material data
uniform MaterialStruct Material;

// 1.0 when x == y, 0.0 otherwise
float when_eq(float x, float y) {
	return 1.0 - abs(sign(x - y));
}

// 1.0 when x < y; 0.0 otherwise
float when_lt(float x, float y) {
	return min(1.0 - sign(x - y), 1.0);
}

// 1.0 when x > y; 0.0 otherwise
float when_ge(float x, float y) {
	return 1.0 - when_lt(x, y);
}

void main() {
	vec3 diffuse = vec3(0.74, 0.74, 0.74);
	float specular = 0.3;

	// Store the per-fragment normals and material shininess into the gbuffer
	gNormal.rgb = normalize(Normal);

	// store diffuse colour and specular component
	gDiffuse.rgb = diffuse;

	// gMatProps.r = Material.shininess;
	// gMatProps.g = specular;

	gMatProps.b = TexCoords.x;
}
