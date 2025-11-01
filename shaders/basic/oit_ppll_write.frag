#version 450
layout(early_fragment_tests) in;

// --- UNIFORMS (Identical to opaque shader) ---
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

// --- FRAGMENT PUSH CONSTANT (Identical) ---
layout(push_constant) uniform FragPushConstants {
    layout(offset = 64)
    vec4 base_color_factor;
    vec4 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    float occlusion_strength;
    float specular_factor;
    vec3 specular_color_factor;
} material;

// --- INPUTS (Identical) ---
layout(location = 6) in vec3 fragWorldPos;
layout(location = 7) in vec3 fragWorldNormal;
layout(location = 8) in vec2 fragTexCoord;
layout(location = 9) in mat3 fragTBN;
layout(location = 12) in vec3 fragColor;
layout(location = 13) in vec2 fragTexCoord1;
layout(location = 14) in float fragInTangentW;

// --- OIT PPLL BUFFERS (Bound at Set 1) ---

// Struct for a single fragment node
struct FragmentNode {
    vec4 color; // .a is alpha
    uint depth;
    uint next;  // Index of the next node
};

// Binding 0: Atomic counter for allocating new fragment indices
layout(set = 1, binding = 0, std430) buffer AtomicCounter {
    uint fragmentCount;
} atomicCounter;

// Binding 1: SSBO to store all fragment data
layout(set = 1, binding = 1, std430) buffer FragmentList {
    FragmentNode fragments[];
} fragmentList;

// Binding 2: Image to store the "head" of each pixel's list
// We use 0xFFFFFFFF as the "null" pointer
layout(set = 1, binding = 2, r32ui) uniform uimage2D startOffsetImage;

// --- PBR Constants and Functions (Identical to opaque shader) ---
const float PI = 3.14159265359;
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};
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
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


// --- Main Function ---
void main() {
    // --- 1. Get Material Properties ---
    vec4 albedo_tex = texture(albedoSampler, fragTexCoord);
    vec3 albedo = albedo_tex.rgb * material.base_color_factor.rgb * fragColor; 
    float alpha = albedo_tex.a * material.base_color_factor.a;

    // --- Discard fully transparent fragments ---
    if (alpha < 0.01) {
        discard;
    }

    vec2 mr = texture(metallicRoughnessSampler, fragTexCoord).bg; 
    float metallic = mr.x * material.metallic_factor;
    float roughness = mr.y * material.roughness_factor;
    float ao = texture(occlusionSampler, fragTexCoord).r * material.occlusion_strength;
    vec3 emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissive_factor.xyz;

    // --- 2. Calculate Base Vectors ---
    vec3 V = normalize(ubo.cameraPos - fragWorldPos);
    vec3 N;               
    if(fragInTangentW != 0){
        vec3 normal_from_map = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0;
        N = normalize(fragTBN * normal_from_map);
    }
    else{
        N = normalize(fragWorldNormal);
    }
    float NdotV = max(dot(N, V), 0.0);

    // --- 3. Calculate Base Layer F0 ---
    vec3 F0_dielectric = 0.08 * material.specular_factor * material.specular_color_factor;
    vec3 F0 = mix(F0_dielectric, albedo, metallic);

    // --- 4. Define Lights ---
    const int NUM_POINT_LIGHTS = 64;
    PointLight pointLights[NUM_POINT_LIGHTS];
    vec3 lightColor = vec3(1.0, 0.85, 0.7);
    float lightIntensity = 4000.0;
    float lightY = 75;
    float startZ = 280.0;
    float spacingZ = -70.0;
    for (int i = 0; i < 8; i++) { pointLights[i] = PointLight(vec3(-200.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 8] = PointLight(vec3(-150.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 16] = PointLight(vec3(-100.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 24] = PointLight(vec3(-50.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 32] = PointLight(vec3(50.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 40] = PointLight(vec3(100.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 48] = PointLight(vec3(150.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }
    for (int i = 0; i < 8; i++) { pointLights[i + 56] = PointLight(vec3(200.0, lightY, startZ + i * spacingZ), lightColor, lightIntensity); }


    // --- 5. Calculate Lighting ---
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < NUM_POINT_LIGHTS; i++)
    {
        vec3 L = normalize(pointLights[i].position - fragWorldPos);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float distance = length(pointLights[i].position - fragWorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
        
        float NDF_base = D_GGX(N, H, roughness);
        float G_base = G_Smith(N, V, L, roughness);
        vec3 F_base = F_Schlick(HdotV, F0);
        
        vec3 kS_base = F_base;
        vec3 kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic);
        
        vec3 numerator_base = NDF_base * G_base * F_base;
        float denominator_base = 4.0 * NdotV * NdotL + 0.001;
        vec3 specular_base = numerator_base / denominator_base;
        vec3 diffuse_base = (kD_base * albedo / PI);
        
        vec3 combined_lighting = (diffuse_base + specular_base);
        Lo += combined_lighting * radiance * NdotL;
    }
    
    // --- 6. Final Color (Linear, Pre-Tonemap) ---
    vec3 ambient = vec3(0.01) * albedo * ao;
    vec3 color = ambient + Lo + emissive;

    // --- 7. PPLL Insertion ---
    
    // Atomically increment the global counter to get a unique index for this fragment
    uint index = atomicAdd(atomicCounter.fragmentCount, 1);
    
    uint max_fragments = fragmentList.fragments.length();
    if (index >= max_fragments) {
        return; // Not enough space, discard fragment
    }

    // Get the current "head" pointer for this pixel
    ivec2 pixel_coord = ivec2(gl_FragCoord.xy);
    uint old_head = imageAtomicExchange(startOffsetImage, pixel_coord, index);

    // Write our fragment data into the list
    fragmentList.fragments[index].color = vec4(color, alpha); // Store linear color
    
    // --- FIX: Use 1.0 / gl_FragCoord.w as the depth key ---
    // This is linear view-space depth (clip.w) and is much more stable
    // than the non-linear gl_FragCoord.z.
    fragmentList.fragments[index].depth = floatBitsToUint(gl_FragCoord.w);
    
    fragmentList.fragments[index].next = old_head; // Our "next" points to the previous head
}

