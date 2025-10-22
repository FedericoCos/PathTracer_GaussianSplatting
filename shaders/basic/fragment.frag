#version 450

// --- UNIFORMS ---
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;

layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metallicRoughnessSampler;
layout(binding = 4) uniform sampler2D occlusionSampler;
layout(binding = 5) uniform sampler2D emissiveSampler;
layout(binding = 6) uniform sampler2D clearcoatSampler;
layout(binding = 7) uniform sampler2D clearcoatRoughnessSampler;

// --- FRAGMENT PUSH CONSTANT ---
layout(push_constant) uniform FragPushConstants {
    layout(offset = 64)
    vec4 base_color_factor;
    vec4 emissive_factor; // .a is unused padding
    float metallic_factor;
    float roughness_factor;
    float occlusion_strength;
    float specular_factor;
    vec3 specular_color_factor;
    float clearcoat_factor;
    float clearcoat_roughness_factor;
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
// IOR 1.5 = F0 0.04
const vec3 F0_CLEARCOAT = vec3(0.04); 

// --- Light Struct ---
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

// --- PBR Helper Functions (Cook-Torrance BRDF) ---
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / (denom + 0.0001); // Epsilon to avoid divide by zero
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

// --- Main Function ---
void main() {
    // --- 1. Get Material Properties ---
    vec4 albedo_tex = texture(albedoSampler, fragTexCoord);
    // (FIXED) No pow(2.2) here, sampler does sRGB->linear conversion
    vec3 albedo = albedo_tex.rgb * material.base_color_factor.rgb * fragColor; 
    float alpha = albedo_tex.a * material.base_color_factor.a;

    // (FIXED) .bg for glTF standard (Roughness=G, Metallic=B)
    vec2 mr = texture(metallicRoughnessSampler, fragTexCoord).bg; 
    float metallic = mr.x * material.metallic_factor;
    float roughness = mr.y * material.roughness_factor;
    
    float ao = texture(occlusionSampler, fragTexCoord).r * material.occlusion_strength;
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissive_factor.xyz;

    // Clearcoat
    float cc_factor = texture(clearcoatSampler, fragTexCoord).r * material.clearcoat_factor;
    float cc_roughness = texture(clearcoatRoughnessSampler, fragTexCoord).g * material.clearcoat_roughness_factor;
    
    // --- 2. Calculate Base Vectors ---
    vec3 V = normalize(ubo.cameraPos - fragWorldPos); // View
    vec3 N = normalize(fragWorldNormal);              // Base normal
    
    // Apply normal map
    vec3 normal_from_map = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0;
    N = normalize(fragTBN * normal_from_map);
    float NdotV = max(dot(N, V), 0.0);
    
    // --- 3. Calculate Base Layer F0 (Reflectivity) ---
    vec3 F0_dielectric = 0.08 * material.specular_factor * material.specular_color_factor;
    vec3 F0 = mix(F0_dielectric, albedo, metallic);

    // --- 4. Define Lights ---
    // (Sun)
    vec3 directionalLightDir = normalize(vec3(0.5, -1.0, -0.75)); // Direction light travels
    vec3 directionalLightColor = vec3(1.0, 1.0, 1.0) * 5.0; // Strong white light
    
    // (Point Lights for testing)
    // Feel free to change these values!
    const int NUM_POINT_LIGHTS = 3;
    PointLight pointLights[NUM_POINT_LIGHTS];
    pointLights[0] = PointLight(vec3(0, 150, -200), vec3(1.0, 0.0, 0.0), 25000.0); // Red
    pointLights[1] = PointLight(vec3(0, 150, -600), vec3(0.0, 1.0, 0.0), 25000.0); // Green
    pointLights[2] = PointLight(vec3(0, 150, -1000), vec3(0.0, 0.0, 1.0), 25000.0); // Blue

    // --- 5. Calculate Lighting ---
    vec3 Lo = vec3(0.0); // "Radiance Out" (final color)

    // --- (A) Directional Light ---
    vec3 L = -directionalLightDir; // Vector to light
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    vec3 radiance = directionalLightColor;

    // Base Layer BRDF
    float NDF_base = D_GGX(N, H, roughness);
    float G_base = G_Smith(N, V, L, roughness);
    vec3 F_base = F_Schlick(HdotV, F0);
    vec3 kS_base = F_base;
    vec3 kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic);
    vec3 numerator_base = NDF_base * G_base * F_base;
    float denominator_base = 4.0 * NdotV * NdotL + 0.001;
    vec3 specular_base = numerator_base / denominator_base;
    vec3 diffuse_base = (kD_base * albedo / PI);
    
    // Clearcoat Layer BRDF
    float NDF_cc = D_GGX(N, H, cc_roughness);
    float G_cc = G_Smith(N, V, L, cc_roughness);
    vec3 F_cc = F_Schlick(HdotV, F0_CLEARCOAT);
    vec3 numerator_cc = NDF_cc * G_cc * F_cc;
    float denominator_cc = 4.0 * NdotV * NdotL + 0.001;
    vec3 specular_cc = numerator_cc / denominator_cc;

    // Combine Layers
    vec3 combined_lighting = (diffuse_base + specular_base) * (1.0 - cc_factor * F_cc) + (specular_cc * cc_factor);
    Lo += combined_lighting * radiance * NdotL;


    // --- (B) Point Lights Loop ---
    for(int i = 0; i < NUM_POINT_LIGHTS; i++)
    {
        // Recalculate L, H, radiance, and NdotL for each light
        L = normalize(pointLights[i].position - fragWorldPos);
        H = normalize(V + L);
        NdotL = max(dot(N, L), 0.0);
        HdotV = max(dot(H, V), 0.0);
        
        // Attenuation
        float distance = length(pointLights[i].position - fragWorldPos);
        float attenuation = 1.0 / (distance * distance);
        radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
        
        // Base Layer BRDF
        NDF_base = D_GGX(N, H, roughness);
        G_base = G_Smith(N, V, L, roughness);
        F_base = F_Schlick(HdotV, F0);
        kS_base = F_base;
        kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic);
        numerator_base = NDF_base * G_base * F_base;
        denominator_base = 4.0 * NdotV * NdotL + 0.001;
        specular_base = numerator_base / denominator_base;
        diffuse_base = (kD_base * albedo / PI);
        
        // Clearcoat Layer BRDF
        NDF_cc = D_GGX(N, H, cc_roughness);
        G_cc = G_Smith(N, V, L, cc_roughness);
        F_cc = F_Schlick(HdotV, F0_CLEARCOAT);
        numerator_cc = NDF_cc * G_cc * F_cc;
        denominator_cc = 4.0 * NdotV * NdotL + 0.001;
        specular_cc = numerator_cc / denominator_cc;

        // Combine Layers
        combined_lighting = (diffuse_base + specular_base) * (1.0 - cc_factor * F_cc) + (specular_cc * cc_factor);
        Lo += combined_lighting * radiance * NdotL;
    }
    
    // --- 6. Final Color ---
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo + emissive;
    
    // HDR to LDR (Tone Mapping) + Gamma Correction
    color = color / (color + vec3(1.0)); // Basic Reinhard tone mapping
    color = pow(color, vec3(1.0/2.2));   // Linear to sRGB
    
    outColor = vec4(color, alpha);
}