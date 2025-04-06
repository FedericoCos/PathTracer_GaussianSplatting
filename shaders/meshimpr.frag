#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

void main() 
{
    vec3 N = normalize(inNormal);
    vec3 V = normalize(-sceneData.view[3].xyz); // View direction (camera position)
    vec3 L = normalize(sceneData.sunlightDirection.xyz);
    vec3 H = normalize(V + L); // Halfway vector

    vec4 texColor = texture(colorTex, inUV);
    vec3 albedo = inColor * texColor.xyz;

    float metallic = materialData.metal_rough_factors.x;
    float roughness = materialData.metal_rough_factors.y;

    vec3 F0 = vec3(0.04); // Default reflectance for non-metals
    F0 = mix(F0, albedo, metallic); // Metals use albedo as F0

    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F; // Specular reflection
    vec3 kD = vec3(1.0) - kS; // Diffuse reflection
    kD *= 1.0 - metallic; // Metals have no diffuse component

    float lightValue = max(dot(N, L), 0.1);
    vec3 diffuse = kD * albedo * lightValue * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

    vec3 ambient = normalize(sceneData.ambientColor.xyz) * sceneData.ambientColor.w * albedo;

    vec3 finalColor = diffuse + specular + ambient;
    
    outFragColor = vec4(finalColor, texColor.a);
}
