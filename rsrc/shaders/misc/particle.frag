// FRAGMENT
#version 400 core

/// output color buffer
out vec4 outColor;

// Inputs from vertex shader
in vec2 outTexCoords;
in vec3 outTint;
in float outAlpha;

// bound to the texture atlas for particle system textures
uniform sampler2D particleTex;

void main() {
    // outColor = vec4(outTexCoords, 0, outAlpha);
    vec4 sampled = texture(particleTex, outTexCoords);
    outColor = vec4(sampled.rgb * outTint, sampled.a * outAlpha);
}

