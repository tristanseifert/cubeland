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
    /// normal texture atlas coordinate
    vec2 NormalUv;
    /// tangent-bitangent-normal matrix for per block normal mapping
    mat3 TBN;
    /// surface normal
    vec3 Normal;
} fs_in;

// info needed to sample the block data texture
flat in ivec2 BlockInfoPos;

/// normal mode: 0 for vertex interpolated, 1 for sampled
flat in ivec2 NormalFlags;

uniform sampler2D blockTypeDataTex;

// texture atlases
uniform sampler2D blockTexAtlas;
uniform sampler2D materialTexAtlas;
uniform sampler2D normalTexAtlas;

void main() {
    // sample textures
    vec4 diffuse = texture(blockTexAtlas, fs_in.DiffuseUv);
    if(diffuse.a == 0) {
        discard;
    }

    vec2 matProps = texture(materialTexAtlas, fs_in.MaterialUv).rg;

    // handle normals
    gNormal = vec4(normalize(fs_in.Normal), 1);

    // calculate normal mapping, if needed
    if(NormalFlags.x == 1) {
        vec3 normal = texture(normalTexAtlas, fs_in.NormalUv).rgb;
        normal = normalize(normal * 2.0 - 1.0);

        normal = normalize(fs_in.TBN * normal);
        gNormal = vec4(normal, 1);
    }

    // Store material properties
    gDiffuse = diffuse;
    gMatSpec = vec4(matProps, 0, 1);
}

