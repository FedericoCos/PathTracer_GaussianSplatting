#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

// Function to apply toon shading levels
float toonShade(float intensity) {
    if (intensity > 0.8) return 1.0;   // Brightest
    if (intensity > 0.6) return 0.8;
    if (intensity > 0.4) return 0.6;
    if (intensity > 0.2) return 0.4;
    return 0.2;  // Darkest
}

void main() 
{
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);
    
    // Compute base lighting
    float lightIntensity = max(dot(normal, lightDir), 0.0);
    lightIntensity = toonShade(lightIntensity); // Apply toon shading bands

    // Sample texture
    vec4 texColor = texture(colorTex, inUV);
    vec3 baseColor = inColor * texColor.xyz;

    // Compute lighting
    vec3 ambient = normalize(sceneData.ambientColor.xyz) * sceneData.ambientColor.w;
    vec3 diffuse = baseColor * lightIntensity * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

    // Final output color
    outFragColor = vec4(diffuse + ambient, texColor.a);
}
