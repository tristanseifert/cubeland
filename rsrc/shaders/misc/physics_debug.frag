// FRAGMENT
#version 400 core

// The normal/shininess buffer is colour attachment 0
layout (location = 0) out vec4 gNormal;
// The albedo/specular buffer is colour attachment 1
layout (location = 1) out vec4 gDiffuse;
// The albedo/specular buffer is colour attachment 2
layout (location = 2) out vec4 gMatSpec;

// Inputs from vertex shader
in vec4 VertexColor;

void main() {
    gDiffuse = VertexColor;
    gMatSpec = vec4(0, 0, 1, 1);
}

