// FRAGMENT
#version 400 core

// inputs from vertex shader
in vec2 TexCoord;

// Output occlusion value (blurred)
layout (location = 0) out float FragColor;

// input occlusion values
uniform sampler2D occlusion;

void main()  {
    vec2 texelSize = 1.0 / vec2(textureSize(occlusion, 0));
    float result = 0.0;
    for(int x = -2; x < 2; ++x) {
        for(int y = -2; y < 2; ++y)  {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(occlusion, TexCoord + offset).r;
        }
    }
    FragColor = result / (4.0 * 4.0);
}
