// FRAGMENT
#version 400 core

void main() {
    // nothing is done since we don't actually render anything (only depth)
    gl_FragDepth = gl_FragCoord.z;
}
