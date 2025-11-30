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

float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
float a2 = a * a;
    float denom = (max(dot(N, H), 0.0) * max(dot(N, H), 0.0) * (a2 - 1.0) + 1.0);
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

// --- SAMPLING HELPERS ---
void getOrthonormalBasis(in vec3 N, out vec3 T, out vec3 B) {
    vec3 up = abs(N.z) < 0.999 ?
vec3(0, 0, 1) : vec3(1, 0, 0);
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
float cosTheta = sqrt((1.0 - r2) / (1.0 + (a*a - 1.0) * r2));
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
    vec3 originOffset = origin + L * 0.01;
// Offset to clear acne
    shadowPayload.isHit = true; 
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, originOffset, 0.0, L, dist - 0.01, 1);
    return shadowPayload.isHit ? 0.0 : 1.0;
}

void sampleLights(vec3 hit_pos, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, float transmission, inout vec3 Lo) {
    // [Assuming your sampleLights logic is here]
    uint num_lights = light_cdf.entries.length();
    if (num_lights == 0) return;
    // ... insert existing light sampling code ...
}

hitAttributeEXT vec2 hitAttrib;
// --- MAIN ---
void main()
{
    MeshInfo info = all_mesh_info.info[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    MaterialData mat = all_materials.materials[info.material_index];
    
    // --- VERTEX INTERPOLATION ---
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
    
    // --- PBR PROPERTIES ---
    float metallic = mat.metallic_factor;
    float roughness = mat.roughness_factor;
    if (mat.mr_id >= 0) {
        vec2 mr = sampleTexture(mat.mr_id, tex_coord).bg;
        metallic *= mr.x;
        roughness *= mr.y;
    }
    // FIX: Removed the duplicate PBR fetch that was down here in previous versions

    // --- TRANSPARENCY LOGIC ---
    float transmission = mat.transmission_factor;

    // A. MASK MODE (Explicit)
    if (mat.alpha_cutoff > 0.0) {
        if (alpha < mat.alpha_cutoff) {
            // DISCARD (Hole)
            payload.hit_flag = 2.0;
            payload.color = vec3(0.0);
            payload.weight = vec3(1.0);
            payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001;
            payload.next_ray_dir = gl_WorldRayDirectionEXT;
            return;
        }
        // SOLID
        alpha = 1.0; 
        transmission = 0.0;
    }
    // B. BLEND MODE (Ambiguous)
    else if (mat.pad > 0.5) {
        // HEURISTIC: Glass is smooth, Leaves are rough.
        // With the double-sampling bug fixed, Roughness will be higher for leaves, 
        // preventing them from entering the glass block.
        bool is_likely_glass = (roughness < 0.2); 
        
        if (is_likely_glass) {
            // Glass Logic
            transmission = max(transmission, 1.0 - alpha);
        } else {
            // Leaf Logic (Stochastic Discard)
            if (alpha < rnd(payload.seed)) {
                 payload.hit_flag = 2.0;
                 payload.color = vec3(0.0);
                 payload.weight = vec3(1.0);
                 payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001;
                 payload.next_ray_dir = gl_WorldRayDirectionEXT;
                 return;
            }
            // SOLID
            alpha = 1.0;
            transmission = 0.0; 
        }
    }
    // C. OPAQUE MODE
    else {
        alpha = 1.0;
        transmission = 0.0;
    }

    // --- REMAINDER OF PBR SETUP ---
    payload.hit_pos = hit_pos;
    payload.normal = N;
    
    // FIX: Occlusion/Emissive logic remains, but metallic/roughness sampling removed
    float ao = mat.occlusion_strength;
    if (mat.occlusion_id >= 0) ao *= sampleTexture(mat.occlusion_id, tex_coord).r;
    vec3 emissive = mat.emissive_factor_and_pad.xyz;
    if (mat.emissive_id >= 0) emissive *= sampleTexture(mat.emissive_id, tex_coord).rgb;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // --- DIRECT LIGHTING ---
    vec3 Lo = vec3(0.0);
    // Only calculate direct lighting if not fully transparent/refractive
    if (transmission < 0.99) {
        sampleLights(hit_pos, N, V, albedo, roughness, metallic, transmission, Lo);
    }
    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao * (1.0 - transmission);
    payload.color = ambient + Lo + emissive;

    // --- NEXT RAY (IMPORTANCE SAMPLING) ---
    payload.hit_flag = 2.0; // Continue path
    payload.next_ray_origin = hit_pos + N_geo * 0.001; 

    // Glass / Translucent Path
    if (transmission > 0.0) {
        vec3 F_val = F_Schlick(abs(dot(N, V)), F0);
        float prob_reflect = max(max(F_val.r, F_val.g), F_val.b);
        
        if (rnd(payload.seed) < prob_reflect) {
            payload.next_ray_dir = reflect(-V, N);
            payload.weight = vec3(1.0);
        } else {
            // Refraction logic
            float refraction_ior = 1.01;
            float eta = (dot(N_geo, V) > 0.0) ? (1.0 / refraction_ior) : refraction_ior;
            vec3 N_refr = (dot(N_geo, V) > 0.0) ? N_geo : -N_geo;
            
            vec3 refractDir = refract(-V, N_refr, eta);
            if (length(refractDir) > 0.0) {
                payload.next_ray_origin = hit_pos - N_geo * 0.001;
                payload.next_ray_dir = refractDir;
                payload.weight = mat.base_color_factor.rgb; // Tint the glass
            } else {
                payload.next_ray_dir = reflect(-V, N);
                payload.weight = vec3(1.0);
            }
        }
        return;
    }

    // Opaque Path (Diffuse vs Specular)
    float prob_specular = mix(0.04, 1.0, metallic);
    prob_specular = mix(prob_specular, 1.0, pow(1.0 - max(dot(N, V), 0.0), 5.0));
    prob_specular = clamp(prob_specular, 0.05, 0.95);

    if (rnd(payload.seed) < prob_specular) {
        vec3 H = sampleGGX(N, roughness, payload.seed);
        vec3 L = reflect(-V, H);
        payload.next_ray_dir = L;
        
        if (dot(N, L) <= 0.0) {
            payload.weight = vec3(0.0);
        } else {
            vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
            payload.weight = F * (1.0 / prob_specular);
        }
    } else {
        vec3 L = sampleCosineHemisphere(N, payload.seed);
        payload.next_ray_dir = L;
        vec3 diffuseColor = albedo * (1.0 - metallic);
        payload.weight = diffuseColor * (1.0 / (1.0 - prob_specular));
    }
}