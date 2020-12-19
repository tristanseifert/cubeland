// FRAGMENT
#version 400 core

layout (location = 0) out vec4 secretion;

void main() {
    // nothing is done since we don't actually render anything (only depth)
    gl_FragDepth = gl_FragCoord.z;
}
