// FRAGMENT
#version 400 core

// The normal/shininess buffer is colour attachment 0
layout (location = 0) out vec4 gNormal;
// The albedo/specular buffer is colour attachment 1
layout (location = 1) out vec4 gDiffuse;
// The albedo/specular buffer is colour attachment 2
layout (location = 2) out vec4 gMatSpec;

// Inputs from vertex shader
in VS_OUT {
    /// world space position of vertex
    vec3 WorldPos;
    /// diffuse texture coordinate
    vec2 DiffuseUv;
    /// material info texture coordinate
    vec2 MaterialUv;
    /// surface normal
    vec3 Normal;
} fs_in;

// info needed to sample the block data texture
flat in ivec2 BlockInfoPos;
uniform sampler2D blockTypeDataTex;

// Samplers (for diffuse and specular)
uniform sampler2D blockTexAtlas;
uniform sampler2D materialTexAtlas;

void main() {
    // Store the per-fragment normals
    gNormal = vec4(normalize(fs_in.Normal), 1);

    // sample textures
    vec4 diffuse = texture(blockTexAtlas, fs_in.DiffuseUv);
    if(diffuse.a == 0) {
        discard;
    }

    vec2 matProps = texture(materialTexAtlas, fs_in.MaterialUv).rg;

    // Store material properties
    gDiffuse = diffuse;
    gMatSpec = vec4(matProps, 0, 1);
}

