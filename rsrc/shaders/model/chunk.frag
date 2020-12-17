// FRAGMENT
#version 400 core

// The normal/shininess buffer is colour attachment 0
layout (location = 0) out vec3 gNormal;
// The albedo/specular buffer is colour attachment 1
layout (location = 1) out vec3 gDiffuse;
// The albedo/specular buffer is colour attachment 2
layout (location = 2) out vec3 gMatSpec;

// Inputs from vertex shader
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

// factor to blend the output with a solid color
uniform float ColorBlendFactor;
// solid color (used when in wireframe mode)
uniform vec3 SolidColor;

// Number of textures to sample (diffuse, specular)
uniform vec2 NumTextures;

// Samplers (for diffuse and specular)
uniform sampler2D texture_diffuse1;

// todo: lol optimize this shizzle yo
void main() {
    // Store the per-fragment normals
    gNormal.rgb = normalize(Normal);

    // Diffuse per-fragment color
    vec3 diffuse = texture(texture_diffuse1, TexCoords).rgb;

    // store diffuse colour and specular component
    // gDiffuse.rgb = (diffuse * (1.0 - ColorBlendFactor)) + (SolidColor * ColorBlendFactor);
    // specular = (specular * (1.0 - ColorBlendFactor)) + (0.74 * ColorBlendFactor);

    // gDiffuse.rgb = vec3(0, 1, 0.5);
    gDiffuse.rgb = diffuse;
    gMatSpec = vec3(0.5, 0.66, ColorBlendFactor);
}
