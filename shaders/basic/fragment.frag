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
// Bindings 6 and 7 (clearcoat) are now removed

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
    // clearcoat_factor and clearcoat_roughness_factor removed
} material;

// --- INPUTS (from vertex shader) ---
layout(location = 6) in vec3 fragWorldPos;
layout(location = 7) in vec3 fragWorldNormal;
layout(location = 8) in vec2 fragTexCoord;
layout(location = 9) in mat3 fragTBN;
layout(location = 12) in vec3 fragColor; // Vertex color
layout(location = 13) in vec2 fragTexCoord1;
layout(location = 14) in float fragInTangentW;

// --- OUTPUT ---
layout(location = 0) out vec4 outColor;

// --- PBR Constants ---
const float PI = 3.14159265359;

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
    
    // Albedo uses fragTexCoord (TEXCOORD_1)
    vec4 albedo_tex = texture(albedoSampler, fragTexCoord); // <-- MODIFIED
    vec3 albedo = albedo_tex.rgb * material.base_color_factor.rgb * fragColor; 
    float alpha = albedo_tex.a * material.base_color_factor.a;

    // Data maps use fragTexCoord (TEXCOORD_0)
    vec2 mr = texture(metallicRoughnessSampler, fragTexCoord).bg; 
    float metallic = mr.x * material.metallic_factor;
    float roughness = mr.y * material.roughness_factor;
    
    float ao = texture(occlusionSampler, fragTexCoord).r * material.occlusion_strength;

    // Emissive likely uses fragTexCoord (same as Albedo)
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissive_factor.xyz; // <-- MODIFIED

    // Clearcoat maps removed
    
    // --- 2. Calculate Base Vectors ---
    vec3 V = normalize(ubo.cameraPos - fragWorldPos); // View
    vec3 N;               // Base normal
    
    // Normal map uses fragTexCoord (TEXCOORD_0)
    if(fragInTangentW != 0){
        vec3 normal_from_map = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0;
        N = normalize(fragTBN * normal_from_map);
    }
    else{
        N = normalize(fragWorldNormal); 
    }
    float NdotV = max(dot(N, V), 0.0);
    
    // --- 3. Calculate Base Layer F0 (Reflectivity) ---
    vec3 F0_dielectric = 0.08 * material.specular_factor * material.specular_color_factor;
    vec3 F0 = mix(F0_dielectric, albedo, metallic);

    // --- 4. Define Lights ---
    // 64 point lights for a smaller, denser area
    const int NUM_POINT_LIGHTS = 64;
    PointLight pointLights[NUM_POINT_LIGHTS];
    
    vec3 lightColor = vec3(1.0, 0.85, 0.7); // Warmish station overhead light
    float lightIntensity = 4000.0; 
    float lightY = 75; // High up ceiling lights

    // 8 long rows of 8 lights each, now closer together
    float startZ = 280.0;       // Starting Z closer to origin
    float spacingZ = -70.0;    // Reduced Z spacing

    // Row 1 (X = -200)
    for (int i = 0; i < 8; i++) {
        pointLights[i] = PointLight(vec3(-200.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }
    
    // Row 2 (X = -150)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 8] = PointLight(vec3(-150.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }

    // Row 3 (X = -100)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 16] = PointLight(vec3(-100.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }

    // Row 4 (X = -50)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 24] = PointLight(vec3(-50.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }

    // Row 5 (X = 50)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 32] = PointLight(vec3(50.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }

    // Row 6 (X = 100)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 40] = PointLight(vec3(100.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }

    // Row 7 (X = 150)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 48] = PointLight(vec3(150.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }
    
    // Row 8 (X = 200)
    for (int i = 0; i < 8; i++) {
        pointLights[i + 56] = PointLight(vec3(200.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity);
    }


    // --- 5. Calculate Lighting ---
    vec3 Lo = vec3(0.0); // "Radiance Out" (final color)

    // --- Point Lights Loop ---
    for(int i = 0; i < NUM_POINT_LIGHTS; i++)
    {
        // Calculate L, H, radiance, and NdotL for each light
        vec3 L = normalize(pointLights[i].position - fragWorldPos);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        
        // Attenuation
        float distance = length(pointLights[i].position - fragWorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
        
        // --- (A) Base Layer (Metallic/Roughness) ---
        float NDF_base = D_GGX(N, H, roughness);
        float G_base = G_Smith(N, V, L, roughness);
        vec3 F_base = F_Schlick(HdotV, F0);
        
        vec3 kS_base = F_base;
        vec3 kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic);
        
        vec3 numerator_base = NDF_base * G_base * F_base;
        float denominator_base = 4.0 * NdotV * NdotL + 0.001;
        vec3 specular_base = numerator_base / denominator_base;
        vec3 diffuse_base = (kD_base * albedo / PI);
        
        // --- (B) Clearcoat Layer (REMOVED) ---

        // --- (C) Combine Layers (SIMPLIFIED) ---
        vec3 combined_lighting = (diffuse_base + specular_base);
        Lo += combined_lighting * radiance * NdotL;
    }
    
    // --- 6. Final Color ---
    // Add ambient and emissive
    vec3 ambient = vec3(0.01) * albedo * ao; // Reduced ambient for dark scene
    vec3 color = ambient + Lo + emissive;
    
    // HDR to LDR (Tone Mapping) + Gamma Correction
    color = color / (color + vec3(1.0)); // Basic Reinhard tone mapping
    color = pow(color, vec3(1.0/2.2));   // Linear to sRGB
    
    outColor = vec4(color, alpha);
}