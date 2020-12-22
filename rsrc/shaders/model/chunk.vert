// VERTEX
#version 400 core
layout (location = 0) in ivec3 position;
layout (location = 1) in uint blockId;
layout (location = 2) in uint faceId;
layout (location = 3) in uint vertexId;

out vec3 WorldPos;
out vec2 TexCoords;
out vec3 Normal;
flat out ivec2 BlockInfoPos;

uniform mat4 model;
uniform mat4 projectionView; // projection * view
uniform mat3 normalMatrix; // transpose(inverse(mat3(model)))

// vertex normal sampler (Y = face ID, X = vertex offset for face 0-3)
uniform sampler2D vtxNormalTex;
// block type data sampler (X = data type, Y = block type ID)
uniform sampler2D blockTypeDataTex;

void main() {
    // sample the normal texture, and the per block type data texture
    vec3 normal = texelFetch(vtxNormalTex, ivec2(0, faceId), 0).rgb;

    // read the texture coordinates. see block registry docs on how this texture is formatted
    BlockInfoPos = ivec2(0, blockId);

    ivec2 uvInfoCoords = ivec2((min(2, faceId) * 2) + vertexId/2, BlockInfoPos.y);

    if(vertexId == 0 || vertexId == 2) { // odd indices are the first two components
        TexCoords = texelFetch(blockTypeDataTex, uvInfoCoords, 0).st;
    } else {
        TexCoords = texelFetch(blockTypeDataTex, uvInfoCoords, 0).pq;
    }

    // Forward the world position and texture coordinates
    vec4 worldPos = model * vec4(position, 1);
    WorldPos = worldPos.xyz;

    // Set position of the vertex pls
    gl_Position = projectionView * worldPos;

    // Send normals (multiplied by normal matrix)
    Normal = normalMatrix * normal;
}
