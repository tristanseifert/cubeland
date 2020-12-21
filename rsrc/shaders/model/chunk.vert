// VERTEX
#version 400 core
layout (location = 0) in ivec3 position;
layout (location = 1) in uint blockId;
layout (location = 2) in uint faceId;

out vec3 WorldPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 model;
uniform mat4 projectionView; // projection * view
uniform mat3 normalMatrix; // transpose(inverse(mat3(model)))

// vertex normal sampler (Y = face ID, X = vertex offset for face 0-3)
uniform sampler2D vtxNormalTex;
// block type data sampler (X = data type, Y = block type ID)
uniform sampler2D blockTypeDataTex;

void main() {
    // sample the normal texture, and the per block type data texture
    uint face = (faceId & 0xF0U) >> 4;
    uint idx = (faceId & 0x0FU);

    vec2 faceDataUv = vec2(float(idx) / 4.0, float(face) / 6);
    vec3 normal = texture(vtxNormalTex, faceDataUv).rgb;

    // Forward the world position and texture coordinates
    vec4 worldPos = model * vec4(position, 1);
    WorldPos = worldPos.xyz;
    // TexCoords = texCoords;
    TexCoords = vec2(0, 1);

    // Set position of the vertex pls
    gl_Position = projectionView * worldPos;

    // Send normals (multiplied by normal matrix)
    Normal = normalMatrix * normal;
}
