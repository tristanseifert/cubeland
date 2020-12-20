// FRAGMENT
#version 400 core
in vec3 pos;
in vec3 fsun;

// layout (location = 0) out vec3 FragColor;
// TODO: Change this back to vec3 for HDR render buffer?
layout (location = 0) out vec4 FragColor;

uniform float time = 0.0;
uniform float cirrus = 0.4;
uniform float cumulus = 0.8;
uniform int numCumulus = 3;

uniform vec3 scatterCoeff = vec3(0.0025, 0.0003, 0.9800);
uniform vec3 nitrogen = vec3(0.650, 0.570, 0.475);

// cloud velocity factors; x = cirrus, y = cumulus
uniform vec2 cloudVelocities;

// noise texture
uniform sampler2D noiseTex;

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

float fbm(vec3 x) {
    float v = 0.0;
    float a = 0.5;
    vec3 shift = vec3(100);
    for (int i = 0; i < 4; ++i) {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

// just sample our noise texture
float fbmTex(vec3 pos) {
    return texture(noiseTex, pos.xz).r;
}


void main() {
    /*if(pos.y < 0) {
      discard;
    }*/

    // calculate constants
    float Br = scatterCoeff.x;
    float Bm = scatterCoeff.y;
    float g =  scatterCoeff.z;

    vec3 Kr = Br / pow(nitrogen, vec3(4.0));
    vec3 Km = Bm / pow(nitrogen, vec3(0.84));

    vec3 color = FragColor.rgb;

    // Atmosphere Scattering
    float mu = dot(normalize(pos), normalize(fsun));
    vec3 extinction = mix(exp(-exp(-((pos.y + fsun.y * 4.0) * (exp(-pos.y * 16.0) + 0.1) / 80.0) / Br) * (exp(-pos.y * 16.0) + 0.1) * Kr / Br) * exp(-pos.y * exp(-pos.y * 8.0 ) * 4.0) * exp(-pos.y * 2.0) * 4.0, vec3(1.0 - exp(fsun.y)) * 0.2, -fsun.y * 0.2 + 0.5);
    color.rgb = 3.0 / (8.0 * 3.14) * (1.0 + mu * mu) * (Kr + Km * (1.0 - g * g) / (2.0 + g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5)) / (Br + Bm) * extinction;

    // Cirrus Clouds
    float density = smoothstep(1.0 - cirrus, 1.0, fbm(pos.xyz / pos.y * 2.0 + time * cloudVelocities.x)) * 0.3;
    color.rgb = mix(color.rgb, extinction * 4.0, density * max(pos.y, 0.0));

    // Cumulus Clouds
    for(int i = 0; i < numCumulus; i++) {
        float density = smoothstep(1.0 - cumulus, 1.0, fbm((0.7 + float(i) * 0.01) * pos.xyz / pos.y + time * cloudVelocities.y));
        color.rgb = mix(color.rgb, extinction * density * 5.0, min(density, 1.0) * max(pos.y, 0.0));
    }

    // Dithering Noise
    color.rgb += noise(pos * 1000) * 0.01;
    FragColor = vec4(color, 1);
}
