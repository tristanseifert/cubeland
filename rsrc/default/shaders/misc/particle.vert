// VERTEX
#version 400 core

/// X/Y coord for the vertex
layout (location = 0) in vec3 position;
/// absolute UV coord for this vertex
layout (location = 1) in vec2 absUv;

/// particle position
layout (location = 2) in vec3 particlePos;
/// particle specific UV coord; expressed as (top left, bottom right)
layout (location = 3) in vec4 particleUv;
/// scale factor for particle size
layout (location = 4) in float scale;
/// alpha for the particle
layout (location = 5) in float alpha;
/// per particle tint
layout (location = 6) in vec3 particleTint;

/// UV coords to shader
out vec2 outTexCoords;
/// desired particle color
out vec3 outTint;
/// desired particle alpha
out float outAlpha;

/// projection matrix
uniform mat4 projectionView;
/// camera right vector in world space
uniform vec3 cameraRightWs;
/// camera up vector in world space
uniform vec3 cameraUpWs;

void main() {
    // compute texture coordinate
    vec2 uv = absUv;

    uv = vec2(max(uv.x, particleUv.x), max(uv.y, particleUv.y)); // pick the top left coordinate if input is 0
    uv = vec2(min(uv.x, particleUv.z), min(uv.y, particleUv.w)); // pick bottom right coordinates if input is 1
    // uv *= particleUv.zw; // scale the bottom right corners

    outTexCoords = uv;

    // forward some other information to the fragment shader
    outAlpha = alpha;
    outTint = particleTint;

    // convert to world space position, and always face the camera
    vec3 pos_ws = particlePos + cameraRightWs * position.x * scale + cameraUpWs * position.y * scale;
    gl_Position = projectionView * vec4(pos_ws, 1);
}
