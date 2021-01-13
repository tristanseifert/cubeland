// FRAGMENT
#version 400 core

precision mediump float;
#define PI 3.1415926535897932384626433832795

// input texture coordinate from vertex shader
in vec2 TexCoord;
// output fragment color
layout (location = 0) out vec4 FragColor;
// current time, in seconds
uniform float time = 0;
// scaling factor
uniform vec2 viewport;

void main() {
    float v = 0.0;
    vec2 c = TexCoord * viewport - viewport/2.0;
    v += sin((c.x+time));
    v += sin((c.y+time)/2.0);
    v += sin((c.x+c.y+time)/2.0);
    c += viewport/2.0 * vec2(sin(time/3.0), cos(time/2.0));
    v += sin(sqrt(c.x*c.x+c.y*c.y+1.0)+time);
    v = v/2.0;
    // vec3 col = vec3(1, sin(PI*v), cos(PI*v));
    vec3 col = vec3(sin(PI*v), sin(PI*v + 2*PI/3), sin(PI*v + 4*PI/3));
    FragColor = vec4(col*.5 + .5, 1);
}
