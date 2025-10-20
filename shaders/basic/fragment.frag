#version 450

// --- BINDINGS ---
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos; // <-- NEW: Camera's world position
} ubo;

layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metallicRoughnessSampler;
layout(binding = 4) uniform sampler2D occlusionSampler;
layout(binding = 5) uniform sampler2D emissiveSampler;

// --- FRAGMENT PUSH CONSTANT ---
layout(push_constant) uniform FragPushConstants {
    layout(offset = 64)
    vec4 base_color_factor;      // 16 bytes
    vec4 emissive_factor; // use vec4 to align (rgb + pad)
    float metallic_factor;       // 4 bytes (but next 12 bytes pad)
    float roughness_factor;      // 4 bytes
    float occlusion_strength;    // 4 bytes
    // pad to multiple of 16 bytes if needed on CPU side
} material;

// --- INPUTS (from vertex shader) ---
layout(location = 5) in vec3 fragWorldPos;
layout(location = 6) in vec3 fragWorldNormal;
layout(location = 7) in vec2 fragTexCoord;
layout(location = 8) in mat3 fragTBN;
layout(location = 11) in vec3 fragColor; // Vertex color

// --- OUTPUT ---
layout(location = 0) out vec4 outColor;

// --- PBR Constants ---
const float PI = 3.14159265359;

// --- PBR Helper Functions (Cook-Torrance BRDF) ---
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = G_SchlickGGX(NdotL, roughness);
    float ggx2 = G_SchlickGGX(NdotV, roughness);
    return ggx1 * ggx2;
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- Light Struct ---
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

// --- Main Function ---
void main() {
    // --- 1. Get Material Properties ---
    vec4 albedo_tex = texture(albedoSampler, fragTexCoord);
    // Combine texture, material factor, and vertex color
    vec3 albedo = pow(albedo_tex.rgb * material.base_color_factor.rgb * fragColor, vec3(2.2));
    float alpha = albedo_tex.a * material.base_color_factor.a;

    vec2 mr = texture(metallicRoughnessSampler, fragTexCoord).rg;
    float metallic = mr.x * material.metallic_factor;
    float roughness = mr.y * material.roughness_factor;
    
    float ao = texture(occlusionSampler, fragTexCoord).r * material.occlusion_strength;
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissive_factor.xyz;

    // --- 2. Calculate Base Vectors ---
    vec3 V = normalize(ubo.cameraPos - fragWorldPos); // View vector
    vec3 N = normalize(fragWorldNormal); // Base normal
    
    // Apply normal map
    vec3 normal_from_map = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0;
    N = normalize(fragTBN * normal_from_map);
    
    // --- 3. Calculate F0 (Base Reflectivity) ---
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // --- 4. Define Lights (Hardcoded) ---
    PointLight pointLights[3];
    pointLights[0] = PointLight(vec3(0.0, 5.0, 5.0), vec3(1.0, 0.0, 0.0), 50.0); // Red
    pointLights[1] = PointLight(vec3(-5.0, 5.0, 0.0), vec3(0.0, 1.0, 0.0), 50.0); // Green
    pointLights[2] = PointLight(vec3(5.0, 5.0, 0.0), vec3(0.0, 0.0, 1.0), 50.0); // Blue
    
    vec3 directionalLightDir = normalize(vec3(1.0, -1.0, -1.0));
    vec3 directionalLightColor = vec3(1.0, 1.0, 1.0) * 1.0; // White sun

    // --- 5. Calculate Lighting ---
    vec3 Lo = vec3(0.0); // Lo = "Radiance Out" (final color)

    // --- Directional Light ---
    vec3 L = -directionalLightDir;
    vec3 H = normalize(V + L);
    vec3 radiance = directionalLightColor;
    
    // Cook-Torrance BRDF
    float NDF = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    // --- Point Lights ---
    for (int i = 0; i < 3; ++i) {
        L = normalize(pointLights[i].position - fragWorldPos);
        H = normalize(V + L);
        
        float distance = length(pointLights[i].position - fragWorldPos);
        float attenuation = 1.0 / (distance * distance);
        radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
        
        NDF = D_GGX(N, H, roughness);
        G = G_Smith(N, V, L, roughness);
        F = F_Schlick(max(dot(H, V), 0.0), F0);
        
        kS = F;
        kD = vec3(1.0) - kS;
        kD *= (1.0 - metallic);
        
        numerator = NDF * G * F;
        denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
        specular = numerator / denominator;
        
        NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // --- 6. Final Color ---
    // Add ambient (approximated by IBL later) and emissive
    vec3 ambient = vec3(0.03) * albedo * ao; // Basic ambient
    vec3 color = ambient + Lo + emissive;

    // HDR to LDR (Tone Mapping) + Gamma Correction
    color = color / (color + vec3(1.0)); // Basic Reinhard tone mapping
    color = pow(color, vec3(1.0/2.2));   // Linear to sRGB
    
    outColor = vec4(color, alpha);
}