// FRAGMENT
#version 400 core

// The normal/shininess buffer is colour attachment 0
layout (location = 0) out vec4 gNormal;
// The albedo/specular buffer is colour attachment 1
layout (location = 1) out vec4 gDiffuse;
// The albedo/specular buffer is colour attachment 2
layout (location = 2) out vec4 gMatSpec;

// Inputs from vertex shader
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

// info needed to sample the block data texture
flat in ivec2 BlockInfoPos;
uniform sampler2D blockTypeDataTex;

// Samplers (for diffuse and specular)
uniform sampler2D blockTexAtlas;

void main() {
    // Store the per-fragment normals
    gNormal = vec4(normalize(Normal), 1);

    // sample textures
    vec3 diffuse = texture(blockTexAtlas, TexCoords).rgb;

    // Store material properties
    gDiffuse = vec4(diffuse, 1);
    gMatSpec = vec4(0.66, 0, 0, 1);
}

