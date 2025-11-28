#version 450
layout(early_fragment_tests) in;

struct PointLight {
    vec4 position;
    vec4 color;
};
// --- UNIFORMS ---
// --- UNIFORMS ---
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec4 ambientLight;
    PointLight[150] pointlights;
    PointLight[150] shadowLights;
    int cur_num_pointlights;
    int cur_num_shadowlights;
    int panelShadowsEnabled;
    float shadowFarPlane;
} ubo;

layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metallicRoughnessSampler;
layout(binding = 4) uniform sampler2D occlusionSampler;
layout(binding = 5) uniform sampler2D emissiveSampler;
layout(binding = 6) uniform sampler2D transmissionSampler;
layout(binding = 7) uniform sampler2D clearcoatSampler;
layout(binding = 8) uniform sampler2D clearcoatRoughnessSampler;
layout(binding = 9) uniform samplerCubeShadow shadowMaps[10];

// --- FRAGMENT PUSH CONSTANT ---
layout(push_constant) uniform FragPushConstants {
    layout(offset = 64)
    vec4 base_color_factor;
    vec4 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    float occlusion_strength;
    float specular_factor;
    vec3 specular_color_factor;
    float alpha_cutoff;
    float transmission_factor;
    float clearcoat_factor;
    float clearcoat_roughness_factor;
} material;

// --- INPUTS ---
layout(location = 6) in vec3 fragWorldPos;
layout(location = 7) in vec3 fragWorldNormal;
layout(location = 8) in vec2 fragTexCoord;
layout(location = 9) in mat3 fragTBN;
layout(location = 12) in vec3 fragColor;
layout(location = 13) in vec2 fragTexCoord1;
layout(location = 14) in float fragInTangentW;
// --- OIT PPLL BUFFERS (Bound at Set 1) ---
struct FragmentNode {
    vec4 color;
    uint depth;
    uint next;
};
layout(set = 1, binding = 0, std430) buffer AtomicCounter {
    uint fragmentCount;
} atomicCounter;
layout(set = 1, binding = 1, std430) buffer FragmentList {
    FragmentNode fragments[];
} fragmentList;
layout(set = 1, binding = 2, r32ui) uniform uimage2DMS startOffsetImage;
// --- PBR Constants and Functions ---
const float PI = 3.14159265359;
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / (denom + 0.0001);
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
// Hammon approximation for the Smith-GGX Height-Correlated Visibility term.
// This replaces both the G_Smith function AND the division by (4 * NdotV * NdotL).
float V_SmithGGXCorrelatedFast(float NdotV, float NdotL, float roughness) {
    float a = roughness;
    float ggxV = NdotL * (NdotV * (1.0 - a) + a);
    float ggxL = NdotV * (NdotL * (1.0 - a) + a);
    
    // The 0.5 factor comes from the 1/(4 * ...) part of the BRDF.
    // The max() is to prevent division by zero.
    return 0.5 / max(ggxV + ggxL, 0.0001);
}
vec3 F_Schlick(float cosTheta, vec3 F0) {
    float x = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x5 = x * x; // x^2
    x5 = x5 * x5 * x; // x^4 * x = x^5
    return F0 + (1.0 - F0) * x5;
}

float calculateShadow(vec3 fragWorldPos, vec3 lightPos, float farPlane, int shadowIndex, vec3 N, vec3 L) {
    float shadowBias = max(0.05 * (1.0 - dot(N, L)), 0.005); 

    vec3 lightToFrag = fragWorldPos - lightPos;
    
    float currentDepth = length(lightToFrag);
    float normalizedDepth = currentDepth / farPlane;
    
    float shadow = texture(shadowMaps[shadowIndex], vec4(lightToFrag, normalizedDepth - shadowBias));
    
    return shadow;
}


// --- Main Function ---
void main() {
    // --- 1. Get Material Properties ---
    vec4 albedo_tex = texture(albedoSampler, fragTexCoord);
    vec3 albedo = albedo_tex.rgb * material.base_color_factor.rgb * fragColor; 
    
    float base_alpha = albedo_tex.a * material.base_color_factor.a;
    float transmission = texture(transmissionSampler, fragTexCoord).r * material.transmission_factor;
    float final_alpha = base_alpha * (1.0 - transmission);
    if (final_alpha < 0.01) {
        discard;
    }

    vec2 mr = texture(metallicRoughnessSampler, fragTexCoord).bg; 
    float metallic = mr.x * material.metallic_factor;
    float roughness = mr.y * material.roughness_factor;
    float ao = texture(occlusionSampler, fragTexCoord).r * material.occlusion_strength;
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissive_factor.xyz;
    // --- NEW: Clearcoat Properties ---
    float cc_factor = texture(clearcoatSampler, fragTexCoord).r * material.clearcoat_factor;
    float cc_roughness = texture(clearcoatRoughnessSampler, fragTexCoord).r * material.clearcoat_roughness_factor;

    // --- 2. Calculate Base Vectors ---
    vec3 V = normalize(ubo.cameraPos - fragWorldPos);
    vec3 N;
    vec3 N_geom = normalize(fragWorldNormal); 
    if(fragInTangentW != 0){
        vec3 normal_from_map = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0;
        N = normalize(fragTBN * normal_from_map);
    }
    else{
        N = N_geom;
    }
    float NdotV = max(dot(N, V), 0.0);
    // --- 3. Calculate Base Layer F0 ---
    vec3 F0_dielectric = 0.08 * material.specular_factor * material.specular_color_factor;
    vec3 F0 = mix(F0_dielectric, albedo, metallic);


    // --- 5. Calculate Lighting ---
    vec3 Lo = vec3(0.0);
    // --- MODIFIED ---
    for(int i = 0; i < ubo.cur_num_pointlights; i++)
    {
        // --- MODIFIED ---
        vec3 L = normalize(ubo.pointlights[i].position.xyz - fragWorldPos);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        // --- MODIFIED ---
        float distance = length(ubo.pointlights[i].position.xyz - fragWorldPos);
        float attenuation = 1.0 / (max(distance * distance, 0.0001));
        // --- MODIFIED ---
        vec3 radiance = ubo.pointlights[i].color.rgb * ubo.pointlights[i].color.a * attenuation;
        
        // --- (A) Base Layer (Metallic/Roughness) ---
        float NDF_base = D_GGX(N, H, roughness);
        float Vis_base = V_SmithGGXCorrelatedFast(NdotV, NdotL, roughness);
        vec3 F_base = F_Schlick(HdotV, F0);
        
        vec3 kS_base = F_base;
        vec3 kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic);
        
        vec3 specular_base = NDF_base * Vis_base * F_base;
        // Scale diffuse by (1.0 - transmission)
        vec3 diffuse_base = (kD_base * albedo / PI) * (1.0 - transmission);
        // --- (B) Clearcoat Layer --- // --- NEW ---
        float NDF_coat = D_GGX(N_geom, H, cc_roughness);
        float G_coat   = G_Smith(N_geom, V, L, cc_roughness);
        vec3 F0_coat = vec3(0.04);
        vec3 F_coat = F_Schlick(HdotV, F0_coat);
        vec3 numerator_coat = NDF_coat * G_coat * F_coat;
        float denominator_coat = 4.0 * NdotV * NdotL + 0.001;
        vec3 specular_coat = numerator_coat / denominator_coat;

        // --- (C) Combine Layers --- // --- MODIFIED ---
        vec3 base_lighting = (diffuse_base + specular_base);
        vec3 combined_lighting = base_lighting * (1.0 - cc_factor * F_coat) + specular_coat * cc_factor;

        Lo += combined_lighting * radiance * NdotL;
    }

    int j = 0;
    for(int i = 0; i < ubo.cur_num_shadowlights; i++){
        // --- MODIFIED ---
        vec3 L = normalize(ubo.shadowLights[i].position.xyz - fragWorldPos);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        // --- MODIFIED ---
        float distance = length(ubo.shadowLights[i].position.xyz - fragWorldPos);
        float attenuation = 1.0 / (max(distance * distance, 0.0001));
        // --- MODIFIED ---
        vec3 radiance = ubo.shadowLights[i].color.rgb * ubo.shadowLights[i].color.a * attenuation;
        
        // --- (A) Base Layer (Metallic/Roughness) ---
        float NDF_base = D_GGX(N, H, roughness);
        float Vis_base = V_SmithGGXCorrelatedFast(NdotV, NdotL, roughness);
        vec3 F_base = F_Schlick(HdotV, F0);
        
        vec3 kS_base = F_base;
        vec3 kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic);
        
        vec3 specular_base = NDF_base * Vis_base * F_base;
        vec3 diffuse_base = (kD_base * albedo / PI);
        
        // --- (B) Clearcoat Layer --- // --- NEW ---
        float NDF_coat = D_GGX(N_geom, H, cc_roughness);
        float G_coat   = G_Smith(N_geom, V, L, cc_roughness);
        // F0 for clearcoat is always dielectric (non-metallic).
        // 0.04 is standard for IOR 1.5.
        vec3 F0_coat = vec3(0.04); 
        vec3 F_coat = F_Schlick(HdotV, F0_coat);
        vec3 numerator_coat = NDF_coat * G_coat * F_coat;
        float denominator_coat = 4.0 * NdotV * NdotL + 0.001;
        vec3 specular_coat = numerator_coat / denominator_coat;

        // --- (C) Combine Layers --- // --- MODIFIED ---
        vec3 base_lighting = (diffuse_base + specular_base);
        // Attenuate the base layer by the clearcoat's reflection (F_coat)
        // and add the clearcoat's own specular reflection.
        // Both are scaled by the clearcoat factor.
        vec3 combined_lighting = base_lighting * (1.0 - cc_factor * F_coat) + specular_coat * cc_factor;


        float shadow = 1.0;
        shadow = calculateShadow(fragWorldPos, ubo.shadowLights[i].position.xyz, ubo.shadowFarPlane, i, N, L);


        Lo += combined_lighting * radiance * NdotL * shadow;
    }
    
    // --- 6. Final Color (Linear, Pre-Tonemap) ---
    
    // Scale ambient by (1.0 - transmission)
    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao * (1.0 - transmission);
    vec3 color = ambient + Lo + emissive;

    // --- 7. PPLL Insertion ---
    uint index = atomicAdd(atomicCounter.fragmentCount, 1);
    uint max_fragments = fragmentList.fragments.length();
    if (index >= max_fragments) {
        return;
    }

    ivec2 pixel_coord = ivec2(gl_FragCoord.xy);
    uint old_head = imageAtomicExchange(startOffsetImage, pixel_coord, gl_SampleID, index);

    fragmentList.fragments[index].color = vec4(color, final_alpha);
    fragmentList.fragments[index].depth = floatBitsToUint(gl_FragCoord.w);
    fragmentList.fragments[index].next = old_head;
}