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
    /// surface normal
    vec3 Normal;
} vs_out;

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
    } else {
        vs_out.DiffuseUv = texelFetch(blockTypeDataTex, uvInfoCoords, 0).pq;
        vs_out.MaterialUv = texelFetch(blockTypeDataTex, uvInfoCoords + ivec2(12, 0), 0).st;
    }
    vs_out.MaterialUv = vec2(0, 0);
    vs_out.MaterialUv = vs_out.DiffuseUv;

    // Forward the world position and texture coordinates
    vec3 posConverted = vec3(position) / vec3(0x7F);
    vec4 worldPos = model * vec4(posConverted, 1);
    vs_out.WorldPos = worldPos.xyz;

    // Set position of the vertex pls
    gl_Position = projectionView * worldPos;

    // Send normals (multiplied by normal matrix)
    vs_out.Normal = normalMatrix * normal;
}
