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

// when 1, we write to the color buffers
uniform float WriteColor;
// highlight color
uniform vec3 HighlightColor;


void main() {
    // write to G buffer only if desired
    if(WriteColor > 0) {
        gDiffuse = vec4(HighlightColor, 1);
        gMatSpec.b = 1; // pass-through without lighting flag
    }
}
