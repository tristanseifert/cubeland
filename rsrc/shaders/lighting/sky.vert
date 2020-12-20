// VERTEX
#version 400 core
out vec3 pos;
out vec3 fsun;

uniform vec3 sunPosition;
uniform mat4 projection;
uniform mat4 view;
uniform float time = 0.0;

const vec2 data[4] = vec2[](
    vec2(-1.0,  1.0), vec2(-1.0, -1.0),
    vec2( 1.0,  1.0), vec2( 1.0, -1.0));

void main() {
    gl_Position = vec4(data[gl_VertexID], 1.0, 1.0);
    pos = transpose(mat3(view)) * (inverse(projection) * gl_Position).xyz;
    fsun = sunPosition;
}
