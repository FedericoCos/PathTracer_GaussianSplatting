#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "raytracing.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

// Binding 2: Output Hit Buffer
layout(set = 0, binding = 2, scalar) buffer writeonly OutputHitBuffer {
    HitData hits[];
} output_buffer;

// Binding 4: Main UBO
layout(set = 0, binding = 4) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec4 ambientLight;
    PointLight pointlights[100];
    PointLight shadowLights[100];
    int cur_num_pointlights;
    int cur_num_shadowlights;
    int panelShadowsEnabled;
    float shadowFarPlane;
} ubo;

// Binding 8: Shadow Maps
layout(set = 0, binding = 8) uniform samplerCubeShadow shadowMaps[10];

// --- Helper: Explicit LOD Sampling (Matches Base Level of Raster) ---
vec4 sampleTexture(int texture_id, vec2 uv) {
    if (texture_id < 0) return vec4(1.0);
    // Using LOD 0 matches the maximum detail. 
    // Note: Rasterization uses Mipmaps (LOD > 0) at distance. 
    // To perfectly match Raster blur at distance, you would need Ray Cones,
    // but LOD 0 is the closest match for near-camera geometry.
    return textureLod(global_textures[nonuniformEXT(texture_id)], uv, 0.0);
}

// --- PBR MATH (Identical to fragment.frag) ---
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

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- SHADOW CALCULATION (Matches fragment.frag logic) ---
float calculateShadow(vec3 fragWorldPos, vec3 lightPos, float farPlane, int shadowIndex, vec3 N, vec3 L) {
    float shadowBias = max(0.05 * (1.0 - dot(N, L)), 0.005);
    vec3 lightToFrag = fragWorldPos - lightPos;
    float currentDepth = length(lightToFrag);
    float normalizedDepth = currentDepth / farPlane;
    return texture(shadowMaps[shadowIndex], vec4(lightToFrag, normalizedDepth - shadowBias));
}

hitAttributeEXT vec2 hitAttrib;

void main()
{
    // --- 1. Geometry & Indices ---
    vec3 hit_pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    
    MeshInfo info = all_mesh_info.info[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = all_materials.materials[info.material_index]; 

    uint i0 = all_indices.i[info.index_offset + gl_PrimitiveID * 3 + 0];
    uint i1 = all_indices.i[info.index_offset + gl_PrimitiveID * 3 + 1];
    uint i2 = all_indices.i[info.index_offset + gl_PrimitiveID * 3 + 2];

    InputVertex v0 = all_vertices.v[info.vertex_offset + i0];
    InputVertex v1 = all_vertices.v[info.vertex_offset + i1];
    InputVertex v2 = all_vertices.v[info.vertex_offset + i2];

    // --- 2. Interpolation ---
    const vec3 bary = vec3(1.0 - hitAttrib.x - hitAttrib.y, hitAttrib.x, hitAttrib.y);
    
    vec2 tex_coord = v0.tex_coord * bary.x + v1.tex_coord * bary.y + v2.tex_coord * bary.z;
    vec3 vertex_color = v0.color * bary.x + v1.color * bary.y + v2.color * bary.z;
    vec3 normal_obj = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    vec4 tangent_obj = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;

    // --- 3. Normal Calculation ---
    // Correctly handle non-uniform scaling by using Inverse-Transpose
    mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    vec3 N_geo = normalize(normalMatrix * normal_obj);

    // Double-Sided / Backface Correction
    // (Rasterization effectively does this by interpolating normals that face the camera 
    //  if cull mode is None, or strictly by geometry)
    vec3 V = -gl_WorldRayDirectionEXT; 
    if (dot(N_geo, V) < 0.0) {
        N_geo = -N_geo;
    }
    
    vec3 N = N_geo;

    // --- 4. Normal Mapping ---
    // Match fragment.frag check: if(fragInTangentW != 0)
    if (abs(tangent_obj.w) > 0.001 && mat.normal_id >= 0) {
        vec3 T = normalize(vec3(gl_ObjectToWorldEXT * vec4(tangent_obj.xyz, 0.0)));
        T = normalize(T - dot(T, N_geo) * N_geo); // Gram-Schmidt
        
        // Use sign(w) to ensure handedness is either 1 or -1, matching Raster behavior
        // where w is usually passed as a flat attribute or uniform.
        vec3 B = cross(N_geo, T) * sign(tangent_obj.w);
        mat3 TBN = mat3(T, B, N_geo);

        vec3 normal_map = sampleTexture(mat.normal_id, tex_coord).rgb;
        normal_map = normal_map * 2.0 - 1.0;
        
        N = normalize(TBN * normal_map);
    }

    // --- 5. Material Properties ---
    vec3 albedo = mat.base_color_factor.rgb * vertex_color;
    float alpha = mat.base_color_factor.a;

    if (mat.albedo_id >= 0) {
        vec4 tex_color = sampleTexture(mat.albedo_id, tex_coord);
        albedo *= tex_color.rgb;
        alpha *= tex_color.a;
    }

    if (alpha < mat.alpha_cutoff) {
        uint index = payload.vertex_index;
        output_buffer.hits[index].hit_pos = vec3(0.0);
        output_buffer.hits[index].hit_flag = -1.0;
        output_buffer.hits[index].color = vec4(0.0);
        return;
    }

    float metallic = mat.metallic_factor; 
    float roughness = mat.roughness_factor; 
    if (mat.mr_id >= 0) {
        vec2 mr = sampleTexture(mat.mr_id, tex_coord).bg;
        metallic *= mr.x;
        roughness *= mr.y;
    }

    float ao = mat.occlusion_strength;
    if (mat.occlusion_id >= 0) {
         float ao_sample = sampleTexture(mat.occlusion_id, tex_coord).r;
         ao *= ao_sample;
    }

    vec3 emissive = mat.emissive_factor_and_pad.xyz; 
    if (mat.emissive_id >= 0) {
        emissive *= sampleTexture(mat.emissive_id, tex_coord).rgb;
    }
    
    float cc_factor = mat.clearcoat_factor; 
    float cc_roughness = mat.clearcoat_roughness_factor;

    // --- 6. Lighting Setup ---
    vec3 F0_dielectric = 0.08 * mat.specular_factor * mat.specular_color_factor;
    vec3 F0 = mix(F0_dielectric, albedo, metallic); 
    vec3 Lo = vec3(0.0);

    #define CALCULATE_LIGHT(lightSource, shadowFactor) \
    { \
        vec3 L = normalize(lightSource.position.xyz - hit_pos); \
        vec3 H = normalize(V + L); \
        float NdotL = max(dot(N, L), 0.0); \
        float HdotV = max(dot(H, V), 0.0); \
        float distance = length(lightSource.position.xyz - hit_pos); \
        float attenuation = 1.0 / (distance * distance); \
        vec3 radiance = lightSource.color.rgb * lightSource.color.a * attenuation; \
        \
        float NDF_base = D_GGX(N, H, roughness); \
        float G_base = G_Smith(N, V, L, roughness); \
        vec3 F_base = F_Schlick(HdotV, F0); \
        vec3 kS_base = F_base; \
        vec3 kD_base = (vec3(1.0) - kS_base) * (1.0 - metallic); \
        vec3 numerator_base = NDF_base * G_base * F_base; \
        float denominator_base = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001; \
        vec3 specular_base = numerator_base / denominator_base; \
        vec3 diffuse_base = (kD_base * albedo / PI); \
        \
        float NDF_coat = D_GGX(N, H, cc_roughness); \
        float G_coat = G_Smith(N, V, L, cc_roughness); \
        vec3 F0_coat = vec3(0.04); \
        vec3 F_coat = F_Schlick(HdotV, F0_coat); \
        vec3 numerator_coat = NDF_coat * G_coat * F_coat; \
        float denominator_coat = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001; \
        vec3 specular_coat = numerator_coat / denominator_coat; \
        \
        vec3 combined_lighting = (diffuse_base + specular_base) * (1.0 - cc_factor * F_coat) + specular_coat * cc_factor; \
        Lo += combined_lighting * radiance * NdotL * shadowFactor; \
    }

    // --- 7. Accumulate Lights ---
    for(int i = 0; i < ubo.cur_num_pointlights; i++) {
        CALCULATE_LIGHT(ubo.pointlights[i], 1.0);
    }
    for(int i = 0; i < ubo.cur_num_shadowlights; i++) {
        vec3 L = normalize(ubo.shadowLights[i].position.xyz - hit_pos);
        // FIX: Use hit_pos exactly as is. No geometric bias.
        // This ensures we sample the shadow map at the exact same coordinate as the Raster shader.
        float shadowFactor = calculateShadow(hit_pos, ubo.shadowLights[i].position.xyz, ubo.shadowFarPlane, i, N, L);
        CALCULATE_LIGHT(ubo.shadowLights[i], shadowFactor);
    }

    // --- 8. Final Output ---
    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao;
    vec3 color = ambient + Lo + emissive; 

    uint index = payload.vertex_index;
    output_buffer.hits[index].hit_pos = hit_pos;
    output_buffer.hits[index].hit_flag = 1.0; 
    output_buffer.hits[index].color = vec4(color, 1.0);
}