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
// vignette paramters (radius, smoothness)
uniform vec2 vignetteParams = vec2(.5, .5);

// uchimura params
uniform vec3 uchimura1;
uniform vec3 uchimura2;

vec3 TonemapColor(vec3 x);
vec3 uchimura(vec3 x);
vec3 tonemapFilmic(vec3 x);
vec3 lottes(vec3 x);

vec3 rgb2hsv(vec3 c);
vec3 hsv2rgb(vec3 c);

float vignette(vec2 uv, float radius, float smoothness);

void main() {
    // additively blend the HDR and bloom textures
    vec3 hdrColor = texture(inSceneColors, TexCoords).rgb;
    vec3 bloomColor = texture(inBloomBlur, TexCoords).rgb;
    hdrColor += (bloomColor * bloomFactor);

    // apply HDR and white point
    // hdrColor = TonemapColor(hdrColor * exposure);
    // hdrColor = tonemapFilmic(hdrColor * exposure);
    // hdrColor = lottes(hdrColor * exposure);
    hdrColor = uchimura(hdrColor * exposure);
    hdrColor = hdrColor / uchimura(whitePoint);

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
    float vig = vignette(TexCoords, vignetteParams.x, vignetteParams.y);

    FragColour = vec4(hdrColor, fxaaLuma) * vig;
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

// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
    vec3 w2 = vec3(step(m + l0, x));
    vec3 w1 = vec3(1.0 - w0 - w2);

    vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
    vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
    vec3 L = vec3(m + a * (x - m));

    return T * w0 + L * w1 + S * w2;
}

vec3 uchimura(vec3 x) {
    float P = uchimura1.x;  // max display brightness
    float a = uchimura1.y;  // contrast
    float m = uchimura1.z; // linear section start
    float l = uchimura2.x;  // linear section length
    float c = uchimura2.y; // black
    float b = uchimura2.z;  // pedestal

    return uchimura(x, P, a, m, l, c, b);
}

vec3 tonemapFilmic(vec3 x) {
      const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);

    vec3 X = max(vec3(0.0), x - 0.004);
    vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
    return pow(result, vec3(2.2));
}

vec3 lottes(vec3 x) {
  const vec3 a = vec3(1.6);
  const vec3 d = vec3(0.977);
  const vec3 hdrMax = vec3(8.0);
  const vec3 midIn = vec3(0.18);
  const vec3 midOut = vec3(0.267);

  const vec3 b =
      (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
  const vec3 c =
      (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

  return pow(x, a) / (pow(x, a * d) * b + c);
}

// basic vignette function
float vignette(vec2 uv, float radius, float smoothness) {
    float diff = radius - distance(uv, vec2(0.5, 0.5));
    return smoothstep(-smoothness, smoothness, diff);
}
