// FRAGMENT
// All lighting calculation is done in the fragment shader, pulling information
// from the different G buffer components, that were rendered by an earlier
// rendering pass: the position, normals, albedo and specular colours.
#version 400 core
in vec2 TexCoord;

// Output lighted colours.
// layout (location = 0) out vec3 FragColour;
// TODO: Change this back to vec3 for HDR render buffer?
layout (location = 0) out vec4 FragColour;

// These three textures are rendered into when the geometry is rendered.
uniform sampler2D gNormal;
uniform sampler2D gDiffuse;
uniform sampler2D gMatProps;
uniform sampler2D gDepth;

uniform sampler2D gSunShadowMap;

uniform sampler2D gOcclusion;

// Fog properties
uniform vec3 fogColor; // = vec3(0.5, 0.5, 0.5);
uniform float fogDensity; // = 0.05;
uniform float fogOffset;

// ambient occlusion factor
uniform float ssaoFactor = 1;

// Ambient light
struct AmbientLight {
    float Intensity;

    vec3 Colour;
};

uniform AmbientLight ambientLight;

// Directional, point and spotlight data
struct DirectionalLight {
    // direction pointing FROM light source
    vec3 Direction;

    vec3 DiffuseColour;
    vec3 SpecularColour;
};

const int MAX_NUM_DIRECTIONAL_LIGHTS = 4;
uniform DirectionalLight directionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];

struct PointLight {
    vec3 Position;

    vec3 DiffuseColour;
    vec3 SpecularColour;

    float Linear;
    float Quadratic;
};

const int MAX_NUM_POINT_LIGHTS = 32;
uniform PointLight pointLights[MAX_NUM_POINT_LIGHTS];

struct SpotLight {
    vec3 Position;
    vec3 Direction;

    vec3 DiffuseColour;
    vec3 SpecularColour;

    // cosines of angles
    float InnerCutOff; // when the light begins to fade
    float OuterCutOff; // outside of this angle, no light is produced

    float Linear;
    float Quadratic;
};

const int MAX_NUM_SPOT_LIGHTS = 8;
uniform SpotLight spotLights[MAX_NUM_SPOT_LIGHTS];

// Number of directional, point and spotlights
uniform vec3 LightCount;

// General parameters
uniform vec3 viewPos;

// Inverse projection matrix: view space -> world space
uniform mat4 projMatrixInv;
// Inverse view matrix: clip space -> view space
uniform mat4 viewMatrixInv;
// Light view matrix: world space -> light space
uniform mat4 lightSpaceMatrix;
// contribution of shadow
uniform float shadowContribution;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 sunDirection);
// Calculates the fog distance, given the depth value.
float FogDistanceFromDepth(float depth);
// Reconstructs view space position from depth buffer.
vec4 ViewSpaceFromDepth(float depth);
// Reconstructs the position from the depth buffer.
vec3 WorldPosFromDepth(float depth);
// Samples the SSAO texture while blurring it.
float SampleOcclusion(vec2 uv);

// 1.0 when x > y, 0.0 otherwise
float when_gt(float x, float y) {
    return max(sign(x - y), 0.0);
}

void main() {
    // Get the depth of the fragment and recalculate the position
    float Depth = texture(gDepth, TexCoord).x;
    vec3 FragWorldPos = WorldPosFromDepth(Depth);

    // retrieve the normals
    vec3 Normal = texture(gNormal, TexCoord).rgb;

    // Calculate the view direction
    vec3 viewDir = normalize(viewPos - FragWorldPos);

    // retrieve albedo (diffuse color)
    vec3 Diffuse = texture(gDiffuse, TexCoord).rgb;

    // Retrieve material properties
    vec4 MatProps = texture(gMatProps, TexCoord);
    float Specular = MatProps.g;
    float Shininess = MatProps.r;
    float SkipFactor = MatProps.b;

    // if only the skybox is rendered at a given light, skip lighting
    if(Depth < 1.0) {
        // Ambient lighting
        float AmbientOcclusion = SampleOcclusion(TexCoord);
        vec3 ambient = Diffuse * ambientLight.Intensity * max(mix(1, AmbientOcclusion, ssaoFactor), 0.33);
        vec3 lighting = vec3(0, 0, 0);

        // Directional lights
        for(int i = 0; i < LightCount.x;  ++i) {
            // get some info about the light
            DirectionalLight light = directionalLights[i];

            // TODO: Figure out if this should be normalized
            vec3 lightDir = light.Direction;

            // Diffuse
            vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.DiffuseColour;

            // Specular
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), Shininess);
            vec3 specular = spec * Specular * light.SpecularColour;

            // Output
            lighting += (diffuse + specular);
        }

        // Point lights
        for(int i = 0; i < LightCount.y; ++i) {
            // get some light info
            PointLight light = pointLights[i];
            vec3 lightDir = normalize(light.Position - FragWorldPos);

            // Diffuse
            vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.DiffuseColour;

            // Specular
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), Shininess);

            vec3 specular = spec * Specular * light.SpecularColour;

            // Attenuation
            float distance = length(light.Position - FragWorldPos);
            float attenuation = 1.0 / (1.0 + (light.Linear * distance) +
                                                    (light.Quadratic * distance * distance));

            diffuse *= attenuation;
            specular *= attenuation;

            // Output
            lighting += (diffuse + specular);
        }


        // Multiply all lighting so far by inverse of shadow
        vec3 sunDirection = directionalLights[0].Direction;
        vec4 FragPosLightSpace = lightSpaceMatrix * vec4(FragWorldPos, 1.0f);
        float shadow = ShadowCalculation(FragPosLightSpace, Normal, sunDirection);

        lighting *= 1.0 - clamp(shadow * shadowContribution, 0, 1);


        // Spot lights
        for(int i = 0; i < LightCount.z;  ++i) {
            // get some info about the light
            SpotLight light = spotLights[i];

            // Calculate to see whether we're inside the 'cone of influence'
            vec3 lightDir = normalize(light.Position - FragWorldPos);
            float theta = dot(lightDir, normalize(-light.Direction));

            // We're working with cosines, not angles, so >
            if(theta > light.OuterCutOff) {
                // Diffuse
                vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.DiffuseColour;

                // Specular
                vec3 halfwayDir = normalize(lightDir + viewDir);
                float spec = pow(max(dot(Normal, halfwayDir), 0.0), Shininess);

                vec3 specular = spec * Specular * light.SpecularColour;

                // Spotlight (soft edges)
                float theta = dot(lightDir, normalize(-light.Direction));
                float epsilon = (light.InnerCutOff - light.OuterCutOff);
                float intensity = clamp((theta - light.OuterCutOff) / epsilon, 0.0, 1.0);
                diffuse *= intensity;
                specular *= intensity;

                // Attenuation
                float distance = length(light.Position - FragWorldPos);
                float attenuation = 1.0f / (1.0 + (light.Linear * distance) +
                                                        (light.Quadratic * (distance * distance)));

                diffuse  *= attenuation;
                specular *= attenuation;

                lighting += (diffuse + specular);
            }
        }


        // Add ambient lighting
        lighting += ambient;

        // apply fog as needed
        float fogDist = max(FogDistanceFromDepth(Depth) - fogOffset, 0);

        float fogFactor = 1.0 / exp((fogDist * fogDensity) * (fogDist * fogDensity));
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        vec3 finalColor = mix(fogColor, lighting, fogFactor);


        // output colour of the fragment
        FragColour = (vec4(finalColor, 1) * (1 - SkipFactor)) + (vec4(Diffuse, 1) * SkipFactor);
    } else {
        // If we don't have anything rendered here, output fog colour
        FragColour = vec4(fogColor, 1);
    }

    // DEBUG: Read shadow map
    /* if(TexCoord.x >= 0.5 && TexCoord.y >= 0.5) {
        vec2 coord = vec2((TexCoord.x - 0.5) * 2, (TexCoord.y - 0.5) * 2);

        float depthValue = texture(gSunShadowMap, coord).r;
        FragColour = vec4(vec3(depthValue), 1);
    }*/
}


// Calculates the fog distance, given the depth value.
float FogDistanceFromDepth(float depth) {
    vec4 viewSpace = ViewSpaceFromDepth(depth);
    return length(viewSpace);
}


// Perform shadow calculation pls
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 sunDirection) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(gSunShadowMap, projCoords.xy).r;

    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;

    // Calculate shadow bias
    float bias = max(0.05 * (1.0 - dot(normal, sunDirection)), 0.005);

    // Check whether current frag pos is in shadow
     // float shadow = (currentDepth - bias) > closestDepth  ? 1.0 : 0.0;

    // PCF with 9 samples to make shadows softer
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(gSunShadowMap, 0);

    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(gSunShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    // Ensure that Z value is no larger than 1
    if(projCoords.z > 1.0) {
        shadow = 0.0;
    }

    return shadow;
}


// Get from depth to view space position
vec4 ViewSpaceFromDepth(float depth) {
    float ViewZ = (depth * 2.0) - 1.0;

    // Get clip space
    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, ViewZ, 1);

    // Clip space -> View space
    vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    // Done
    return viewSpacePosition;
}

// this is supposed to get the world position from the depth buffer
vec3 WorldPosFromDepth(float depth) {
    float ViewZ = (depth * 2.0) - 1.0;

    // Get clip space
    vec4 clipSpacePosition = vec4(TexCoord * 2.0 - 1.0, ViewZ, 1);

    // Clip space -> View space
    vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    // View space -> World space
    vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

    return worldSpacePosition.xyz;
}

/**
 * Samples the SSAO buffer, by taking the average of a 4x4 grid of pixels centered around the given
 * UV coordinate. Since during SSAO rendering we have a 4x4 noise texture that's applied, this will
 * remove that noise and improve its appearance.
 */
float SampleOcclusion(vec2 uv) {
    vec2 texelSize = 1.0 / vec2(textureSize(gOcclusion, 0));
    float result = 0.0;
    for(int x = -2; x < 2; ++x) {
        for(int y = -2; y < 2; ++y)  {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(gOcclusion, uv + offset).r;
        }
    }

    return result / (4.0 * 4.0);
}

