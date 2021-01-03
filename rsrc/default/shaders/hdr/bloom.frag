// FRAGMENT
#version 400 core

// texture coordinate to sample from vertex shader
in vec2 TexCoord;

// output the actual colour
layout (location = 0) out vec4 FragColour;
// The input from the last pass of the shader
uniform sampler2D inTex;

// The direction of the blur: (1, 0) for horizontal, (0, 1) for vertical.
uniform vec2 direction;
// Size of texture
uniform vec2 resolution;

// which blur kernel size to use: 5, 9, or 13
uniform int blurKernelSz;


// Performs a 5x5 Gaussian blur.
vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction);
// Performs a 9x9 Gaussian blur.
vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction);
// Performs a 13x13 Gaussian blur.
vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction);

void main() {
	// apply the appropriate sized blur
	if(blurKernelSz == 5) {
		FragColour = blur5(inTex, TexCoord, resolution, direction);
	} else if(blurKernelSz == 9) {
		FragColour = blur9(inTex, TexCoord, resolution, direction);
	} else if(blurKernelSz == 13) {
		FragColour = blur13(inTex, TexCoord, resolution, direction);
	}
}


// Performs a 5x5 Gaussian blur.
vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
	vec4 color = vec4(0.0);
	vec2 off1 = vec2(1.3333333333333333) * direction;

	color += texture(image, uv) * 0.29411764705882354;
	color += texture(image, uv + (off1 / resolution)) * 0.35294117647058826;
	color += texture(image, uv - (off1 / resolution)) * 0.35294117647058826;

	return color;
}

// Performs a 9x9 Gaussian blur.
vec4 blur9(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
	vec4 color = vec4(0.0);

	vec2 off1 = vec2(1.3846153846) * direction;
	vec2 off2 = vec2(3.2307692308) * direction;

	color += texture(image, uv) * 0.2270270270;
	color += texture(image, uv + (off1 / resolution)) * 0.3162162162;
	color += texture(image, uv - (off1 / resolution)) * 0.3162162162;
	color += texture(image, uv + (off2 / resolution)) * 0.0702702703;
	color += texture(image, uv - (off2 / resolution)) * 0.0702702703;

	return color;
}

// Performs a 13x13 Gaussian blur.
vec4 blur13(sampler2D image, vec2 uv, vec2 resolution, vec2 direction) {
	vec4 color = vec4(0.0);

	vec2 off1 = vec2(1.411764705882353) * direction;
	vec2 off2 = vec2(3.2941176470588234) * direction;
	vec2 off3 = vec2(5.176470588235294) * direction;

	color += texture(image, uv) * 0.1964825501511404;
	color += texture(image, uv + (off1 / resolution)) * 0.2969069646728344;
	color += texture(image, uv - (off1 / resolution)) * 0.2969069646728344;
	color += texture(image, uv + (off2 / resolution)) * 0.09447039785044732;
	color += texture(image, uv - (off2 / resolution)) * 0.09447039785044732;
	color += texture(image, uv + (off3 / resolution)) * 0.010381362401148057;
	color += texture(image, uv - (off3 / resolution)) * 0.010381362401148057;

	return color;
}
