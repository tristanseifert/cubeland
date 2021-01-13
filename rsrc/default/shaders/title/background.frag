// FRAGMENT
#version 400 core

// input texture coordinate from vertex shader
in vec2 TexCoord;
// sampler for input texture
uniform sampler2D inTexture;
// output fragment color
layout (location = 0) out vec4 FragColor;

vec3 rgb2hsv(vec3 c);
vec3 hsv2rgb(vec3 c);

void main() {
    // sample
    vec4 color = texture(inTexture, TexCoord);

    // HSV transform
    vec3 hsv = rgb2hsv(color.rgb);

    // hsv.y *= .75;
    hsv.z *= .5;

    // output
    FragColor = vec4(hsv2rgb(hsv), color.a);
}

// Converts an RGB pixel to HSV
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    // vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    // vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);
    vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
// Converts a HSV pixel to RGB
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
