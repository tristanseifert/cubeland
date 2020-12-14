// FRAGMENT
#version 400 core

// Material data
struct MaterialStruct {
    // how reflective the material is: lower value = more reflective
    float shininess;
};

// The normal/shininess buffer is colour attachment 0
layout (location = 0) out vec3 gNormal;
// The albedo/specular buffer is colour attachment 1
layout (location = 1) out vec3 gDiffuse;
// The albedo/specular buffer is colour attachment 2
layout (location = 2) out vec4 gMatSpec;

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

// 1.0 when x ≥ y; 0.0 otherwise
float when_geq(float x, float y) {
	return max(sign(x - y), 0.0);
}

// todo: lol optimize this shizzle yo
void main() {
	// Store the per-fragment normals
	gNormal.rgb = normalize(Normal);

	// Diffuse per-fragment color
	vec3 diffuse = texture(texture_diffuse1, TexCoords).rgb;
	diffuse += texture(texture_diffuse2, TexCoords).rgb * when_geq(NumTextures.x, 2);
	diffuse += texture(texture_diffuse3, TexCoords).rgb * when_geq(NumTextures.x, 3);
	diffuse += texture(texture_diffuse4, TexCoords).rgb * when_geq(NumTextures.x, 4);

	// Specular intensity
	float specular = texture(texture_specular1, TexCoords).a * when_geq(NumTextures.y, 1);
	specular += texture(texture_specular2, TexCoords).a * when_geq(NumTextures.y, 2);
	specular += texture(texture_specular3, TexCoords).a * when_geq(NumTextures.y, 3);
	specular += texture(texture_specular4, TexCoords).a * when_geq(NumTextures.y, 4);

	// store diffuse colour and specular component
	gDiffuse.rgb = diffuse;

	// Store material properties
	gMatSpec = vec4(Material.shininess, specular, 1, 0);
}
