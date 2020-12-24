// FRAGMENT
#version 400 core

// inputs from vertex shader
in vec2 TexCoord;

// Output occlusion value
layout (location = 0) out float FragColor;

// These three textures are rendered into when the geometry is rendered.
uniform sampler2D gNormal;
uniform sampler2D gDepth;

// 4x4 noise texture
uniform sampler2D texNoise;

// kernel sample positions
uniform vec3 samples[64];

// SSAO params
uniform int kernelSize = 64;
uniform float radius = 0.5;
uniform float bias = 0.025;

// tile noise texture over screen based on screen dimensions divided by noise size
uniform vec2 noiseScale = vec2(800.0/4.0, 600.0/4.0); 

// projection matrix
uniform mat4 projection;
// Inverse projection matrix: view space -> world space
uniform mat4 projMatrixInv;
// Inverse view matrix: clip space -> view space
uniform mat4 viewMatrixInv;

// tan(FoV / 2)
uniform float thfov;
// aspect ratio (width/height)
uniform float aspect;

// Reconstructs view space position from depth buffer.
vec4 ViewSpaceFromDepth(float depth);
// Reconstructs the position from the depth buffer.
vec3 WorldPosFromDepth(float depth);




void main() {
    // get the world position
    vec2 ndc = (TexCoord * 2.0) - 1.0;
    vec3 vViewRay = vec3(
        ndc.x * thfov * aspect,
        ndc.y * thfov,
        1.0
    );

    float FragWorldDepth = texture(gDepth, TexCoord).x;
    vec3 FragWorldPos = WorldPosFromDepth(FragWorldDepth);
    // vec3 FragWorldPos = vViewRay * texture(gDepth, TexCoord).r;

    // get input for SSAO algorithm
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb);
    vec3 randomVec = normalize(texture(texNoise, TexCoord * noiseScale).xyz);

    // create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i) {
        // get sample position
        // vec3 samplePos = TBN * samples[i]; // from tangent to view-space
        // samplePos = FragWorldPos + samplePos * radius; 

        vec3 samplePos = TBN * samples[i];
        samplePos = samplePos * radius + FragWorldPos;

        // project sample position (to get position on screen/texture)
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = (offset.xyz * 0.5) + 0.5; // transform to range 0.0 - 1.0

        // get sample depth
        float sampleDepth = texture(gDepth, offset.xy).x; // get depth value of kernel sample

        // range check & accumulate
        // float rangeCheck = 1;
        // float rangeCheck = smoothstep(0.0, 1.0, radius / abs(FragWorldDepth - sampleDepth));
        // occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
        float rangeCheck = abs(FragWorldPos.z - sampleDepth) < radius ? 1.0 : 0.0;
        occlusion += (sampleDepth <= samplePos.z ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / kernelSize);

    FragColor = occlusion;
}

// Get from depth to view space position
vec4 ViewSpaceFromDepth(float depth) {
    float ViewZ = (depth * 2.0) - 1.0;

    // Get clip space
    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, ViewZ, 1);

    // Clip space -> View space
    vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    // Done
    return viewSpacePosition;
}

// this is supposed to get the world position from the depth buffer
vec3 WorldPosFromDepth(float depth) {
    float ViewZ = (depth * 2.0) - 1.0;

    // Get clip space
    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, ViewZ, 1);

    // Clip space -> View space
    vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    // View space -> World space
    vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

    return worldSpacePosition.xyz;
}
