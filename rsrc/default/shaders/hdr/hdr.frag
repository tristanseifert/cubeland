// FRAGMENT
#version 400 core

in vec2 TexCoord;

// fragment output colour: this stores the bright parts of the image
layout (location = 0) out vec3 BloomOut;

// This texture contains the contents of the scene, i.e. the HDR buffer
uniform sampler2D texInColour;

// luminance threshold for what colors are considered bright
uniform float lumaThreshold;

float when_gt(float x, float y) {
    return max(sign(x - y), 0.0);
}

void main() {
    // sample the original buffer
    vec3 inColour = texture(texInColour, TexCoord).rgb;

    // galgulate luma
    float luma = dot(inColour, vec3(0.2126, 0.7152, 0.0722));

    // copy the bright colours to the bloom buffer
    BloomOut = inColour.rgb * when_gt(luma, lumaThreshold);
}
