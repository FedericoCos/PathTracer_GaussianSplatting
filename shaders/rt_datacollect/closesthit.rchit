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

// --- TEXTURE & PBR HELPERS ---
vec4 sampleTexture(int texture_id, vec2 uv) {
    if (texture_id < 0) return vec4(1.0);
    return textureLod(global_textures[nonuniformEXT(texture_id)], uv, 0.0);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- SAMPLING HELPERS ---
void getOrthonormalBasis(in vec3 N, out vec3 T, out vec3 B) {
    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

vec3 sampleCosineHemisphere(vec3 N, inout uint seed) {
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float phi = 2.0 * PI * r1;
    float sqrtR2 = sqrt(r2);
    vec3 localDir = vec3(cos(phi) * sqrtR2, sin(phi) * sqrtR2, sqrt(1.0 - r2));
    vec3 T, B;
    getOrthonormalBasis(N, T, B);
    return normalize(T * localDir.x + B * localDir.y + N * localDir.z);
}

vec3 sampleGGX(vec3 N, float roughness, inout uint seed) {
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float a = roughness * roughness;
    float phi = 2.0 * PI * r1;
    
    // Safety clamp to prevent divide by zero
    float denom = (1.0 + (a*a - 1.0) * r2);
    float cosTheta = sqrt((1.0 - r2) / max(denom, 0.0001));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    
    vec3 H_local = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    vec3 T, B;
    getOrthonormalBasis(N, T, B);
    return normalize(T * H_local.x + B * H_local.y + N * H_local.z);
}

// --- SHADOW & LIGHTS ---
float traceShadow(vec3 origin, vec3 lightPos) {
    vec3 L = lightPos - origin;
    float dist = length(L);
    L = normalize(L);
    // FIX: Reduced bias to prevent Peter Panning
    vec3 originOffset = origin + L * 0.001; 
    
    shadowPayload.isHit = true; 
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, originOffset, 0.0, L, dist - 0.001, 1);
    return shadowPayload.isHit ? 0.0 : 1.0;
}

void sampleLights(vec3 hit_pos, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, float transmission, inout vec3 Lo) {
    uint num_lights = light_cdf.entries.length();
    if (num_lights == 0) return;
    // ... [Insert your existing light sampling logic here] ...
}

hitAttributeEXT vec2 hitAttrib;

// --- MAIN ---
void main()
{
    MeshInfo info = all_mesh_info.info[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = all_materials.materials[info.material_index];
    
    // --- 1. VERTEX INTERPOLATION ---
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
    
    // FIX: Normalize tangent immediately after interpolation
    vec4 tangent_obj = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;
    tangent_obj.xyz = normalize(tangent_obj.xyz); 
    
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
    
    vec3 albedo = mat.base_color_factor.rgb * vertex_color;
    float alpha = mat.base_color_factor.a;
    if (mat.albedo_id >= 0) {
        vec4 tex = sampleTexture(mat.albedo_id, tex_coord);
        albedo *= tex.rgb;
        alpha *= tex.a;
    }
    
    // --- 2. UNPACK MATERIAL DATA ---
    // FIX: Read cutoff from PACKED LOCATION (assuming C++ pack to .w)
    // If you revert C++, change this back to mat.alpha_cutoff
    float real_alpha_cutoff = mat.emissive_factor_and_pad.w; 
    
    // --- 3. PBR PROPERTIES ---
    float metallic = mat.metallic_factor;
    float roughness = mat.roughness_factor;
    if (mat.mr_id >= 0) {
        vec2 mr = sampleTexture(mat.mr_id, tex_coord).bg;
        metallic *= mr.x;
        roughness *= mr.y;
    }

    // --- 4. TRANSPARENCY & MASK LOGIC ---
    float transmission = mat.transmission_factor;

    // CASE A: MASK MODE
    if (real_alpha_cutoff > 0.0) {
        if (alpha < real_alpha_cutoff) {
            payload.hit_flag = 2.0;
            payload.color = vec3(0.0);
            payload.weight = vec3(1.0);
            payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001;
            payload.next_ray_dir = gl_WorldRayDirectionEXT;
            return;
        }
        alpha = 1.0; 
        transmission = 0.0;
    }
    // CASE B: BLEND MODE (Glass)
    else if (mat.pad > 0.5) {
        if (alpha < 0.05) { 
             payload.hit_flag = 2.0;
             payload.color = vec3(0.0);
             payload.weight = vec3(1.0);
             payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001;
             payload.next_ray_dir = gl_WorldRayDirectionEXT;
             return;
        }
        transmission = max(transmission, 1.0 - alpha);
    }
    // CASE C: OPAQUE (With Texture Border Fix)
    else {
        // FIX: Force texture cutouts even if material is Opaque.
        // Solves "Black Contour" caused by transparent texture borders.
        if (alpha < 0.005) {
             payload.hit_flag = 2.0; 
             payload.color = vec3(0.0);
             payload.weight = vec3(1.0);
             payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001;
             payload.next_ray_dir = gl_WorldRayDirectionEXT;
             return;
        }
        alpha = 1.0;
        transmission = 0.0;
    }

    // --- 5. LIGHTING & REFLECTION ---
    payload.hit_pos = hit_pos;
    payload.normal = N;
    
    float ao = mat.occlusion_strength;
    if (mat.occlusion_id >= 0) ao *= sampleTexture(mat.occlusion_id, tex_coord).r;
    
    vec3 emissive = mat.emissive_factor_and_pad.xyz;
    if (mat.emissive_id >= 0) emissive *= sampleTexture(mat.emissive_id, tex_coord).rgb;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Direct Light
    vec3 Lo = vec3(0.0);
    if (transmission < 0.99) {
        sampleLights(hit_pos, N, V, albedo, roughness, metallic, transmission, Lo);
    }
    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao * (1.0 - transmission);
    payload.color = ambient + Lo + emissive;

    // Next Ray Setup
    payload.hit_flag = 2.0;
    
    // Glass Path
    if (transmission > 0.0) {
        payload.next_ray_origin = hit_pos - N_geo * 0.001; // Push slightly inside for refraction
        vec3 F_val = F_Schlick(abs(dot(N, V)), F0);
        float prob_reflect = max(max(F_val.r, F_val.g), F_val.b);
        
        if (rnd(payload.seed) < prob_reflect) {
            payload.next_ray_origin = hit_pos + N_geo * 0.001; // Push out for reflection
            payload.next_ray_dir = reflect(-V, N);
            payload.weight = vec3(1.0);
        } else {
            float refraction_ior = 1.01;
            float eta = (dot(N_geo, V) > 0.0) ? (1.0 / refraction_ior) : refraction_ior;
            vec3 N_refr = (dot(N_geo, V) > 0.0) ? N_geo : -N_geo;
            
            vec3 refractDir = refract(-V, N_refr, eta);
            if (length(refractDir) > 0.0) {
                payload.next_ray_dir = refractDir;
                payload.weight = mat.base_color_factor.rgb;
            } else {
                payload.next_ray_origin = hit_pos + N_geo * 0.001;
                payload.next_ray_dir = reflect(-V, N);
                payload.weight = vec3(1.0);
            }
        }
        return;
    }

    // Opaque / Clearcoat Path
    payload.next_ray_origin = hit_pos + N_geo * 0.001;

    // 1. CLEARCOAT SETUP & VALIDATION
    float clearcoat = mat.clearcoat_factor;
    float cc_roughness = mat.clearcoat_roughness_factor;
    
    vec3 H_cc = sampleGGX(N, cc_roughness, payload.seed);
    vec3 L_cc = reflect(-V, H_cc);
    
    // FIX: Geometric check. Clearcoat must reflect OUTWARDS.
    bool cc_valid = (clearcoat > 0.0) && (dot(L_cc, N_geo) > 0.0);

    vec3 F_cc = vec3(0.0);
    float prob_cc = 0.0;

    if (cc_valid) {
        F_cc = F_Schlick(abs(dot(N, V)), vec3(0.04)) * clearcoat;
        prob_cc = max(F_cc.r, max(F_cc.g, F_cc.b));
    }

    // 2. LAYER SELECTION
    if (cc_valid && rnd(payload.seed) < prob_cc) {
        // Hit Clearcoat
        payload.next_ray_dir = L_cc;
        payload.weight = vec3(1.0); 
    } else {
        // Hit Base Layer
        
        // FIX: Energy Attenuation
        // Only apply if clearcoat WAS valid but we passed through it.
        // If cc was invalid (black ring case), we treat it as non-existent (attenuation = 1.0).
        vec3 energy_attenuation = vec3(1.0);
        if (cc_valid) {
             float denominator = max(1.0 - prob_cc, 0.001);
             // Per-channel energy conservation
             energy_attenuation = (vec3(1.0) - F_cc) / denominator;
        }

        float prob_specular = mix(0.04, 1.0, metallic);
        prob_specular = mix(prob_specular, 1.0, pow(1.0 - max(dot(N, V), 0.0), 5.0));
        prob_specular = clamp(prob_specular, 0.05, 0.95);

        if (rnd(payload.seed) < prob_specular) {
            // Specular Base
            vec3 H = sampleGGX(N, roughness, payload.seed);
            vec3 L = reflect(-V, H);
            
            // FIX: Geometric Fallback. If Specular is blocked, weight is 0.
            if (dot(L, N_geo) <= 0.0) {
                payload.weight = vec3(0.0);
            } else {
                payload.next_ray_dir = L;
                vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
                payload.weight = (F * (1.0 / prob_specular)) * energy_attenuation;
            }
        } else {
            // Diffuse Base
            vec3 L = sampleCosineHemisphere(N, payload.seed);
            // Safety
            if (dot(L, N_geo) <= 0.0) L = N_geo; 

            payload.next_ray_dir = L;
            vec3 diffuseColor = albedo * (1.0 - metallic);
            
            payload.weight = (diffuseColor * (1.0 / (1.0 - prob_specular))) * energy_attenuation;
        }
    }
}