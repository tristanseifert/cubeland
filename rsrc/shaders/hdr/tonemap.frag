// FRAGMENT
#version 400 core

in vec2 TexCoords;

// output the actual colour
layout (location = 0) out vec4 FragColour;
// luminance output
layout (location = 1) out vec4 LumaOut;

// Raw scene colour output
uniform sampler2D inSceneColors;
// Blurred bloom parts
uniform sampler2D inBloomBlur;

// white point: the brightest colour
uniform vec3 whitePoint;
// exposure value
uniform float exposure;
// bloom factor
uniform float bloomFactor;
// HSV adjustment for output pixels
uniform vec3 hsvAdjust;

vec3 TonemapColor(vec3 x);

vec3 rgb2hsv(vec3 c);
vec3 hsv2rgb(vec3 c);

void main() {
    // additively blend the HDR and bloom textures
    vec3 hdrColor = texture(inSceneColors, TexCoords).rgb;
    vec3 bloomColor = texture(inBloomBlur, TexCoords).rgb;
    hdrColor += (bloomColor * bloomFactor);

    // apply HDR and white point
    hdrColor = TonemapColor(hdrColor * exposure);
    hdrColor = hdrColor / TonemapColor(whitePoint);

    // HSV adjustments 
    vec3 hsv = rgb2hsv(hdrColor);

    hsv.x += mod(hsvAdjust.x / 360.0, 360);
    hsv.yz *= hsvAdjust.yz;
    hsv.yz = clamp(hsv.yz, 0, 1);
    //hsv.xyz = mod(hsv.xyz, 1.0);
    hdrColor = hsv2rgb(hsv);

    // compute luma for the FXAA shader and for the preceived luma
    float fxaaLuma = dot(hdrColor, vec3(0.299, 0.587, 0.114));
    float preceivedLuma = dot(hdrColor, vec3(0.2126, 0.7152, 0.0722));

    // output
    FragColour = vec4(hdrColor, fxaaLuma);
    LumaOut = vec4(0, 0, preceivedLuma, 1);
}

// Applies the tonemapping algorithm on a colour
vec3 TonemapColor(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
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
