// VERTEX
#version 400 core
layout (location = 0) in ivec3 position;
layout (location = 1) in uint blockId;
layout (location = 2) in uint faceId;
layout (location = 3) in uint vertexId;


out VS_OUT {
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
    /// surface normal (interpolated)
    vec3 Normal;
} vs_out;

/// normal mode: 0 for vertex interpolated, 1 for sampled
flat out ivec2 NormalFlags;

flat out ivec2 BlockInfoPos;

uniform mat4 model;
uniform mat4 projectionView; // projection * view
uniform mat3 normalMatrix; // transpose(inverse(mat3(model)))

// vertex normal sampler (Y = face ID, X = vertex offset for face 0-3)
uniform sampler2D vtxNormalTex;
// block type data sampler (X = data type, Y = block type ID)
uniform sampler2D blockTypeDataTex;

void main() {
    // sample normals
    vec3 normal = texelFetch(vtxNormalTex, ivec2(vertexId, faceId), 0).rgb;

    // read the texture coordinates. see block registry docs on how this texture is formatted
    BlockInfoPos = ivec2(0, blockId);

    uint faceTexId = min(2, faceId);
    ivec2 uvInfoCoords = ivec2((faceId * 2) + vertexId/2, BlockInfoPos.y);

    if(vertexId == 0 || vertexId == 2) { // odd indices are the first two components
        vs_out.DiffuseUv = texelFetch(blockTypeDataTex, uvInfoCoords, 0).st;
        vs_out.MaterialUv = texelFetch(blockTypeDataTex, uvInfoCoords + ivec2(12, 0), 0).st;
        vs_out.NormalUv = texelFetch(blockTypeDataTex, uvInfoCoords + ivec2(25, 0), 0).st;
    } else {
        vs_out.DiffuseUv = texelFetch(blockTypeDataTex, uvInfoCoords, 0).pq;
        vs_out.MaterialUv = texelFetch(blockTypeDataTex, uvInfoCoords + ivec2(12, 0), 0).pq;
        vs_out.NormalUv = texelFetch(blockTypeDataTex, uvInfoCoords + ivec2(25, 0), 0).pq;
    }
    vs_out.MaterialUv = vec2(0, 0);
    vs_out.MaterialUv = vs_out.DiffuseUv;

    // read the "normal map enabled" flag
    vec3 normFlags = texelFetch(blockTypeDataTex, ivec2(24, BlockInfoPos.y), 0).xyz;

    if(normFlags.x >= .5) {
        // read the tangent vector
        vec3 sTangent = texelFetch(vtxNormalTex, ivec2(vertexId + 4, faceId), 0).rgb;

        vec3 T = normalize(vec3(model * vec4(sTangent, 0)));
        vec3 N = normalize(vec3(model * vec4(normal, 0)));

        // calculate bitangent and TBN matrix
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);

        vs_out.TBN = TBN;

        NormalFlags = ivec2(1, 0);
    } else {
        NormalFlags = ivec2(0);
    }

    // Forward the world position and texture coordinates
    vec3 posConverted = vec3(position) / vec3(0x7F);
    vec4 worldPos = model * vec4(posConverted, 1);
    vs_out.WorldPos = worldPos.xyz;

    // Set position of the vertex pls
    gl_Position = projectionView * worldPos;

    // Send normals (multiplied by normal matrix)
    vs_out.Normal = normalMatrix * normal;
}
