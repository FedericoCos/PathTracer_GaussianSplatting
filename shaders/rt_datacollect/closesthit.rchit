#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#define RAY_TRACING
#include "raytracing.glsl"

// --- LOCATIONS ---
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 2) rayPayloadEXT RayPayload payloadRefl;
layout(location = 1) rayPayloadEXT ShadowPayload shadowPayload;

// --- BINDINGS ---


// Note: OutputBuffer and ShadowMaps bindings are REMOVED. 

// --- CONSTANTS ---
const float PI = 3.14159265359;

// --- FUNCTIONS (Keep existing Helper/PBR functions) ---
vec4 sampleTexture(int texture_id, vec2 uv) {
    if (texture_id < 0) return vec4(1.0);
    return textureLod(global_textures[nonuniformEXT(texture_id)], uv, 0.0);
}

float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom + 0.0001);
}

float V_SmithGGXCorrelatedFast(float NdotV, float NdotL, float roughness) {
    float a = roughness;
    float ggxV = NdotL * (NdotV * (1.0 - a) + a);
    float ggxL = NdotV * (NdotL * (1.0 - a) + a);
    return 0.5 / max(ggxV + ggxL, 0.0001);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    // Simplified G_Smith for Clearcoat
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    return ggx1 * ggx2;
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- RAY TRACED SHADOW FUNCTION ---
float traceShadow(vec3 origin, vec3 lightPos) {
    vec3 L = lightPos - origin;
    float dist = length(L);
    L = normalize(L);
    
    // Bias to prevent self-intersection
    vec3 originOffset = origin + L * 0.01; 

    // Initialize shadow payload
    shadowPayload.isHit = true; 

    // Trace Ray
    // gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT:
    // Optimization: Stop as soon as we hit ANYTHING opaque. Don't run CHIT.
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    
    // traceRayEXT(tlas, flags, mask, sbtRecordOffset, sbtRecordStride, missIndex, origin, tmin, dir, tmax, payloadLoc)
    // missIndex = 1 assumes the shadow miss shader is at index 1 in the SBT miss group
    traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, originOffset, 0.0, L, dist, 1);

    if (shadowPayload.isHit) {
        return 0.0; // Occluded
    }
    return 1.0; // Visible
}

hitAttributeEXT vec2 hitAttrib;

void main()
{
    // --- 1. GEOMETRY RECONSTRUCTION (Same as before) ---
    MeshInfo info = all_mesh_info.info[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = all_materials.materials[info.material_index]; 

    uint i0 = all_indices.i[info.index_offset + gl_PrimitiveID * 3 + 0];
    uint i1 = all_indices.i[info.index_offset + gl_PrimitiveID * 3 + 1];
    uint i2 = all_indices.i[info.index_offset + gl_PrimitiveID * 3 + 2];
    
    InputVertex v0 = all_vertices.v[info.vertex_offset + i0];
    InputVertex v1 = all_vertices.v[info.vertex_offset + i1];
    InputVertex v2 = all_vertices.v[info.vertex_offset + i2];

    const vec3 bary = vec3(1.0 - hitAttrib.x - hitAttrib.y, hitAttrib.x, hitAttrib.y);
    
    vec3 hit_pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec2 tex_coord = v0.tex_coord * bary.x + v1.tex_coord * bary.y + v2.tex_coord * bary.z;
    vec3 vertex_color = v0.color * bary.x + v1.color * bary.y + v2.color * bary.z;
    vec3 normal_obj = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    vec4 tangent_obj = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;

    mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    vec3 N_geo = normalize(normalMatrix * normal_obj);
    
    vec3 V = -gl_WorldRayDirectionEXT;
    if (dot(N_geo, V) < 0.0) N_geo = -N_geo;
    vec3 N = N_geo;

    // Normal Mapping
    if (abs(tangent_obj.w) > 0.001 && mat.normal_id >= 0) {
        vec3 T = normalize(vec3(gl_ObjectToWorldEXT * vec4(tangent_obj.xyz, 0.0)));
        T = normalize(T - dot(T, N_geo) * N_geo);
        vec3 B = cross(N_geo, T) * sign(tangent_obj.w);
        mat3 TBN = mat3(T, B, N_geo);
        vec3 normal_map = sampleTexture(mat.normal_id, tex_coord).rgb * 2.0 - 1.0;
        N = normalize(TBN * normal_map);
    }

    // --- 2. MATERIAL PROPERTIES (Same as before) ---
    vec3 albedo = mat.base_color_factor.rgb * vertex_color;
    float alpha = mat.base_color_factor.a;
    if (mat.albedo_id >= 0) {
        vec4 tex = sampleTexture(mat.albedo_id, tex_coord);
        albedo *= tex.rgb;
        alpha *= tex.a;
    }
    
    // Alpha Masking
    if (alpha < mat.alpha_cutoff) {
        // Ignored intersection (should typically be handled in AnyHit, but works here)
        payload.hit_flag = -1.0; 
        payload.color = vec3(0.0);
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
    if (mat.occlusion_id >= 0) ao *= sampleTexture(mat.occlusion_id, tex_coord).r;

    vec3 emissive = mat.emissive_factor_and_pad.xyz;
    if (mat.emissive_id >= 0) emissive *= sampleTexture(mat.emissive_id, tex_coord).rgb;

    // --- 3. LIGHTING CALCULATION ---
    float cc_factor = mat.clearcoat_factor;
    float cc_roughness = mat.clearcoat_roughness_factor;
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Helper macro for light accumulation
    #define ACCUMULATE_LIGHT(lightPos, lightColor, shadowFactor) { \
        vec3 L = normalize(lightPos - hit_pos); \
        vec3 H = normalize(V + L); \
        float NdotL = max(dot(N, L), 0.0); \
        float HdotV = max(dot(H, V), 0.0); \
        float dist = length(lightPos - hit_pos); \
        float atten = 1.0 / (dist * dist); \
        vec3 radiance = lightColor.rgb * lightColor.a * atten; \
        \
        float NDF = D_GGX(N, H, roughness); \
        float Vis = V_SmithGGXCorrelatedFast(NdotV, NdotL, roughness); \
        vec3 F = F_Schlick(HdotV, F0); \
        vec3 kS = F; \
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic); \
        vec3 specular = NDF * Vis * F; \
        vec3 diffuse = kD * albedo / PI; \
        \
        /* Clearcoat */ \
        float NDF_cc = D_GGX(N, H, cc_roughness); \
        float G_cc = G_Smith(N, V, L, cc_roughness); \
        vec3 F_cc = F_Schlick(HdotV, vec3(0.04)); \
        vec3 spec_cc = (NDF_cc * G_cc * F_cc) / (4.0 * NdotV * NdotL + 0.001); \
        \
        vec3 combined = (diffuse + specular) * (1.0 - cc_factor * F_cc) + spec_cc * cc_factor; \
        Lo += combined * radiance * NdotL * shadowFactor; \
    }

    // Loop Point Lights (Standard + Shadow)
    // We treat them identically now, checking real visibility
    for(int i = 0; i < ubo.cur_num_pointlights; i++) {
        float shadow = traceShadow(hit_pos, ubo.pointlights[i].position.xyz);
        ACCUMULATE_LIGHT(ubo.pointlights[i].position.xyz, ubo.pointlights[i].color, shadow);
    }
    for(int i = 0; i < ubo.cur_num_shadowlights; i++) {
        float shadow = traceShadow(hit_pos, ubo.shadowLights[i].position.xyz);
        ACCUMULATE_LIGHT(ubo.shadowLights[i].position.xyz, ubo.shadowLights[i].color, shadow);
    }

    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao;
    vec3 finalColor = ambient + Lo + emissive;

    // --- 4. SIMPLE REFLECTION (Recursive) ---
    // If the material is reflective (metallic or very smooth)
    if (payload.depth < 1) { 
        if (metallic > 0.1 || roughness < 0.2) {
            vec3 reflectDir = reflect(-V, N);
            
            // Setup the reflection payload
            payloadRefl.depth = payload.depth + 1;
            payloadRefl.hit_flag = 0.0;
            payloadRefl.color = vec3(0.0); 

            // Trace using Location 2 (payloadRefl)
            traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, hit_pos + N * 0.001, 0.0, reflectDir, 1000.0, 2); // <--- Note the '2' at the end
            
            if (payloadRefl.hit_flag > 0.0) {
                vec3 F = F_Schlick(NdotV, F0);
                finalColor += payloadRefl.color * F * (1.0 - roughness); 
            } else {
                finalColor += vec3(0.05) * (1.0 - roughness);
            }
        }
    }

    // --- 5. WRITE TO PAYLOAD ---
    payload.hit_pos = hit_pos;
    payload.normal = N;
    payload.color = finalColor;
    payload.hit_flag = 1.0;
}