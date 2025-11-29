#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#define RAY_TRACING
#include "raytracing.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT ShadowPayload shadowPayload;

const float PI = 3.14159265359;

// --- FUNCTIONS ---
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
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float traceShadow(vec3 origin, vec3 lightPos) {
    vec3 L = lightPos - origin;
    float dist = length(L);
    L = normalize(L);
    
    // 1. Increase Offset bias (0.01 -> 0.05) to clear self-intersection
    vec3 originOffset = origin + L * 0.05; 
    
    shadowPayload.isHit = true; 
    
    // 2. Add gl_RayFlagsCullFrontFacingTrianglesEXT ???
    // If we assume lights are always 'outside', maybe cull front faces? No, risky.
    
    // Standard Opaque Flags
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    
    // Decrease max distance slightly to avoid hitting the light itself
    traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, originOffset, 0.0, L, dist - 0.05, 1);
    
    if (shadowPayload.isHit) return 0.0;
    return 1.0;
}

// --- NEW: NEE Sampling ---
// Removed dependency on global 'mat'. Uses passed 'roughness'.
void sampleLights(vec3 hit_pos, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, float transmission, inout vec3 Lo) 
{
    uint num_lights = light_cdf.entries.length();
    // Safety check
    if (num_lights == 0) return;

    float r1 = rnd(payload.seed);
    
    // Binary Search (Protected)
    uint left = 0;
    uint right = num_lights - 1;
    uint idx = 0;
    
    // Limit iterations to prevent infinite loops in case of corrupt data
    for(int i=0; i<32; i++) {
        if(left > right) break;
        uint mid = (left + right) / 2;
        if (light_cdf.entries[mid].cumulative_probability < r1) {
            left = mid + 1;
        } else {
            idx = mid;
            if(mid == 0) break; // Prevent underflow of uint
            right = mid - 1; 
        }
    }
    
    // Clamp index just in case
    idx = min(idx, num_lights - 1);
    
    uint tri_idx = light_cdf.entries[idx].triangle_index;
    LightTriangle tri = light_triangles.tris[tri_idx];
    
    vec3 p0 = all_vertices.v[tri.v0].pos;
    vec3 p1 = all_vertices.v[tri.v1].pos;
    vec3 p2 = all_vertices.v[tri.v2].pos;
    
    float u = rnd(payload.seed);
    float v = rnd(payload.seed);
    if (u + v > 1.0) { u = 1.0 - u; v = 1.0 - v; }
    
    vec3 light_pos = p0 * (1.0 - u - v) + p1 * u + p2 * v;
    vec3 light_normal = normalize(cross(p1 - p0, p2 - p0));
    
    vec3 L = light_pos - hit_pos;
    float dist_sq = dot(L, L);
    float dist = sqrt(dist_sq);
    L /= dist;
    
    float NdotL = max(dot(N, L), 0.0);
    float LdotN_light = dot(-L, light_normal); 
    
    if (NdotL > 0.0 && LdotN_light > 0.0) {
        float visibility = traceShadow(hit_pos, light_pos);
        if (visibility > 0.0) {
            MaterialData l_mat = all_materials.materials[tri.material_index];
            vec3 Le = l_mat.emissive_factor_and_pad.rgb;
            
            float geometry_term = LdotN_light / dist_sq;
            
            vec3 H = normalize(V + L);
            vec3 F0 = mix(vec3(0.04), albedo, metallic);
            
            float NDF = D_GGX(N, H, roughness);
            float Vis = V_SmithGGXCorrelatedFast(dot(N,V), NdotL, roughness); 
            vec3 F = F_Schlick(dot(H, V), F0);
            
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
            vec3 specular = NDF * Vis * F;
            vec3 diffuse = (kD * albedo / PI) * (1.0 - transmission);
            
            vec3 brdf = diffuse + specular;
            float weight = ubo.totalSceneFlux; // Use the real total flux
            Lo += brdf * Le * NdotL * geometry_term * weight * visibility;
        }
    }
}

hitAttributeEXT vec2 hitAttrib;

void main()
{
    // --- GEOMETRY ---
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
    bool is_back_face = dot(N_geo, V) < 0.0;
    if (is_back_face) N_geo = -N_geo;
    vec3 N = N_geo;
    if (abs(tangent_obj.w) > 0.001 && mat.normal_id >= 0) {
        vec3 T = normalize(vec3(gl_ObjectToWorldEXT * vec4(tangent_obj.xyz, 0.0)));
        T = normalize(T - dot(T, N_geo) * N_geo);
        vec3 B = cross(N_geo, T) * sign(tangent_obj.w);
        mat3 TBN = mat3(T, B, N_geo);
        vec3 normal_map = sampleTexture(mat.normal_id, tex_coord).rgb * 2.0 - 1.0;
        N = normalize(TBN * normal_map);
    }
    vec3 albedo = mat.base_color_factor.rgb * vertex_color;
    float alpha = mat.base_color_factor.a;
    if (mat.albedo_id >= 0) {
        vec4 tex = sampleTexture(mat.albedo_id, tex_coord);
        albedo *= tex.rgb;
        alpha *= tex.a;
    }
    
    // --- OUTPUT 1: GEOMETRY CAPTURE ---
    payload.hit_pos = hit_pos;
    payload.normal = N;

    // NOTE: Seed init moved to RayGen!

    if (alpha < mat.alpha_cutoff) {
        payload.hit_flag = -1.0; 
        payload.color = vec3(0.0);
        return; 
    }

    float transmission = max(mat.transmission_factor, 1.0 - alpha);
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

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
    float NdotV = max(dot(N, V), 0.0);

    // --- LIGHTING ---
    // (Existing Point Light Loop skipped for brevity - Insert here if you want Hybrid)
    
    // --- NEE SAMPLING ---
    sampleLights(hit_pos, N, V, albedo, roughness, metallic, transmission, Lo);

    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao * (1.0 - transmission);
    payload.color = ambient + Lo + emissive;

    // --- NEXT RAY ---
    payload.hit_flag = 1.0; 
    payload.weight = vec3(0.0);

    // GLASS / DIELECTRIC TRANSMISSION
    if (transmission > 0.0) {
        float eta = is_back_face ? (1.5 / 1.0) : (1.0 / 1.5);
        vec3 N_refr = is_back_face ? -N : N; 
        
        // Calculate Fresnel for probability
        vec3 F_val = F_Schlick(abs(dot(N, V)), F0);
        float prob_reflect = max(max(F_val.r, F_val.g), F_val.b);
        
        // Stochastic Choice: Reflect or Refract?
        if (rnd(payload.seed) < prob_reflect) {
            // REFLECTION
            vec3 reflectDir = reflect(-V, N);
            payload.next_ray_origin = hit_pos + N_geo * 0.001;
            payload.next_ray_dir = reflectDir;
            // Weight = Color * (1 / PDF)
            // Color ~ F, PDF ~ F -> Weight ~ 1.0
            payload.weight = vec3(1.0); 
            payload.hit_flag = 2.0;
        } else {
            // REFRACTION
            vec3 refractDir = refract(gl_WorldRayDirectionEXT, N_refr, eta);
            if (length(refractDir) > 0.0) {
                payload.next_ray_origin = hit_pos - N_geo * 0.001;
                payload.next_ray_dir = refractDir;
                // Weight = TransmissionColor * (1 / PDF)
                // PDF ~ (1 - F) -> Weight ~ Transmission
                payload.weight = vec3(transmission); // * albedo if colored glass
                payload.hit_flag = 2.0;
            } else {
                // TIR (Total Internal Reflection) -> Force Reflect
                vec3 reflectDir = reflect(-V, N);
                payload.next_ray_origin = hit_pos + N_geo * 0.001;
                payload.next_ray_dir = reflectDir;
                payload.weight = vec3(1.0);
                payload.hit_flag = 2.0;
            }
        }
    }

    // ... (Refl/Refr Logic - SAME) ...
    // Note: Use 'metallic' and 'roughness' (locals), not 'mat.*'
    if (metallic > 0.1 || roughness < 0.2) {
        vec3 reflectDir = reflect(-V, N);
        vec3 F = F_Schlick(NdotV, F0);
        payload.next_ray_origin = hit_pos + N_geo * 0.001;
        payload.next_ray_dir = reflectDir;
        payload.weight = F * (1.0 - roughness); 
        payload.hit_flag = 2.0; 
    }
}