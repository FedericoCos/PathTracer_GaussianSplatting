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

// --- TEXTURE HELPERS ---
vec4 sampleTexture(int texture_id, vec2 uv) {
    if (texture_id < 0) return vec4(1.0);
    return textureLod(global_textures[nonuniformEXT(texture_id)], uv, 0.0); 
}

vec3 safeNormalize(vec3 v) {
    float len = length(v);
    if (len < 1e-6) return vec3(0.0, 1.0, 0.0); // or fallback normal
    return v / len;
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- SAMPLING HELPERS ---
void getOrthonormalBasis(in vec3 N, out vec3 T, out vec3 B) {
    vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0); 
    T = safeNormalize(cross(up, N));
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
    return safeNormalize(T * localDir.x + B * localDir.y + N * localDir.z);
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

vec3 sampleGGX(vec3 N, float roughness, inout uint seed) {
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float a = roughness * roughness; 
    float phi = 2.0 * PI * r1;
    float denom = (1.0 + (a*a - 1.0) * r2); 
    float cosTheta = sqrt((1.0 - r2) / max(denom, 0.0001));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta); 
    vec3 H_local = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    vec3 T, B;
    getOrthonormalBasis(N, T, B);
    return safeNormalize(T * H_local.x + B * H_local.y + N * H_local.z);
}

// --- SHADOW & LIGHTS ---
float traceShadow(vec3 origin, vec3 lightPos) {
    vec3 L = lightPos - origin;
    float dist = length(L); 
    L = safeNormalize(L);
    vec3 originOffset = origin + L * 0.01;
    shadowPayload.isHit = true;
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT; 
    traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, originOffset, 0.0, L, dist - 0.01, 1);
    return shadowPayload.isHit ? 0.0 : 1.0; 
}

void sampleLights_SG(vec3 hit_pos, vec3 N, vec3 V, vec3 albedo, float roughness, vec3 F0, float transmission, inout vec3 Lo)
{
    uint num_lights = light_cdf.entries.length();
    if (num_lights == 0) return;

    float r1 = rnd(payload.seed);

    // Binary Search
    uint left = 0;
    uint right = num_lights - 1; uint idx = 0;
    while (left <= right) {
        uint mid = (left + right) / 2;
        if (light_cdf.entries[mid].cumulative_probability < r1) {
            left = mid + 1;
        } else {
            idx = mid;
            if (mid == 0) break;
            right = mid - 1;
        }
    }

    uint tri_idx = light_cdf.entries[idx].triangle_index;
    LightTriangle tri = light_triangles.tris[tri_idx];

    vec3 p0 = all_vertices.v[tri.v0].pos;
    vec3 p1 = all_vertices.v[tri.v1].pos;
    vec3 p2 = all_vertices.v[tri.v2].pos;

    float u = rnd(payload.seed); float v = rnd(payload.seed);
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
        visibility = max(visibility, transmission);

        if (visibility > 0.0) {
            MaterialData l_mat = all_materials.materials[tri.material_index];
            vec3 Le = l_mat.emissive_factor_and_pad.rgb;
            float geometry_term = LdotN_light / dist_sq;

            vec3 H = normalize(V + L);

            float NDF = D_GGX(N, H, roughness);
            float Vis = V_SmithGGXCorrelatedFast(dot(N,V), NdotL, roughness);
            vec3 F = F_Schlick(dot(H, V), F0);

            vec3 kS = F;
            // FIX: Removed `* (1.0 - metallic)`.
            // In Metal/Rough, `albedo` is already Black for metals (handled in main).
            // In Spec/Gloss, `albedo` is inherently Black for metals.
            // This unifies the logic.
            vec3 kD = (1 - kS) * (1 - transmission);

            vec3 specular = NDF * Vis * F;
            vec3 diffuse = (kD * albedo / PI) * (1.0 - transmission);

            vec3 brdf = diffuse + specular;

            Lo += brdf * Le * NdotL * geometry_term * ubo.totalSceneFlux * ubo.ambientLight.w * visibility;
        }
    }
}

void sampleLights(vec3 hit_pos, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0, float transmission, inout vec3 Lo) 
{
    uint num_lights = light_cdf.entries.length();
    if (num_lights == 0) return;

    float r1 = rnd(payload.seed);
    
    // Binary Search for Light Triangle
    uint left = 0;
    uint right = num_lights - 1; uint idx = 0;
    while (left <= right) {
        uint mid = (left + right) / 2;
        if (light_cdf.entries[mid].cumulative_probability < r1) {
            left = mid + 1;
        } else {
            idx = mid;
            if (mid == 0) break;
            right = mid - 1;
        }
    }
    
    // Get Triangle Geometry
    uint tri_idx = light_cdf.entries[idx].triangle_index;
    LightTriangle tri = light_triangles.tris[tri_idx];
    
    vec3 p0 = all_vertices.v[tri.v0].pos;
    vec3 p1 = all_vertices.v[tri.v1].pos;
    vec3 p2 = all_vertices.v[tri.v2].pos;
    
    // Sample Point on Triangle
    float u = rnd(payload.seed); float v = rnd(payload.seed);
    if (u + v > 1.0) { u = 1.0 - u; v = 1.0 - v; }
    vec3 light_pos = p0 * (1.0 - u - v) + p1 * u + p2 * v;
    vec3 light_normal = safeNormalize(cross(p1 - p0, p2 - p0));
    
    // Lighting Vectors
    vec3 L = light_pos - hit_pos;
    float dist_sq = dot(L, L);
    float dist = sqrt(dist_sq);
    L /= dist;
    
    float NdotL = max(dot(N, L), 0.0);
    float LdotN_light = dot(-L, light_normal); 
    
    if (NdotL > 0.0 && LdotN_light > 0.0) {
        float visibility = traceShadow(hit_pos, light_pos);
        // Shadow Trick: Transparent objects don't fully shadow themselves
        visibility = max(visibility, transmission);
        if (visibility > 0.0) {
            MaterialData l_mat = all_materials.materials[tri.material_index];
            vec3 Le = l_mat.emissive_factor_and_pad.rgb;
            float geometry_term = LdotN_light / dist_sq;
            
            // BRDF Calculations
            vec3 H = safeNormalize(V + L);
            
            // F0 is now passed in, not calculated here
            
            float NDF = D_GGX(N, H, roughness);
            float Vis = V_SmithGGXCorrelatedFast(dot(N,V), NdotL, roughness);
            vec3 F = F_Schlick(dot(H, V), F0);
            
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
            vec3 specular = NDF * Vis * F;
            vec3 diffuse = (kD * albedo / PI) * (1.0 - transmission);
            vec3 brdf = diffuse + specular;
            
            // Add Contribution
            Lo += brdf * Le * NdotL * geometry_term * ubo.totalSceneFlux * ubo.ambientLight.w * visibility;
        }
    }
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
    
    // --- 2. TBN & NORMAL MAPPING ---
    vec3 normal_obj = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    vec4 tangent_obj = v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z;
    // Transform to World Space
    vec3 N_geo = safeNormalize(vec3(gl_ObjectToWorldEXT * vec4(normal_obj, 0.0)));
    vec3 T_geo = safeNormalize(vec3(gl_ObjectToWorldEXT * vec4(tangent_obj.xyz, 0.0))); 
    
    // Gram-Schmidt Orthogonalization
    T_geo = safeNormalize(T_geo - dot(T_geo, N_geo) * N_geo);
    vec3 B_geo = cross(N_geo, T_geo) * sign(tangent_obj.w); 

    vec3 V = -gl_WorldRayDirectionEXT;
    if (dot(N_geo, V) < 0.0) N_geo = -N_geo; 
    
    vec3 N = safeNormalize(N_geo);

   vec3 T = T_geo; 
    
    float Tw = tangent_obj.w;
    
    // Validation check
    // We check Tw (W component) to handle your RT Box case where W=0.0
    bool tangentValid = (abs(Tw) > 0.001) && (length(T) > 0.001);

    if (!tangentValid) {
        // Generate a fallback Tangent if the model doesn't have one (like the RT Box)
        vec3 up = (abs(N.y) < 0.999) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        T = safeNormalize(cross(up, N));
    }

    // Recalculate Bitangent to ensure orthogonality
    // Use explicit check for w < 0 to flip, avoiding sign(0) issues
    float handedness = (Tw < 0.0) ? -1.0 : 1.0;
    vec3 B = safeNormalize(cross(N, T)) * handedness;
    
    mat3 TBN = mat3(T, B, N);

    if (abs(Tw) > 0.0001 && mat.normal_id > 0) {
        vec3 normal_map = sampleTexture(mat.normal_id, tex_coord).xyz * 2.0 - 1.0;
        N = safeNormalize(TBN * normal_map);
    }

    // --- 3. MATERIAL PROPERTIES UNIFICATION ---
    vec3 albedo;
    float alpha;
    vec3 F0;
    float roughness;
    float metallic;

    // WORKFLOW A: SPECULAR - GLOSSINESS
    if (mat.use_specular_glossiness_workflow > 0.5) {
        // 1. Diffuse (Albedo)
        vec4 diffuse_sample = mat.base_color_factor * vec4(vertex_color, 1.0);
        if (mat.albedo_id > 0) {
            diffuse_sample *= sampleTexture(mat.albedo_id, tex_coord);
        }
        albedo = diffuse_sample.rgb;
        alpha  = diffuse_sample.a;

        // 2. Specular & Glossiness
        vec3 specular_color = mat.specular_color_factor;
        float glossiness = mat.roughness_factor; // We stored glossiness here in C++

        if (mat.sg_id > 0) {
            vec4 sg_tex = sampleTexture(mat.sg_id, tex_coord);
            specular_color *= sg_tex.rgb; // RGB is Specular Color
            glossiness *= sg_tex.a;       // A is Glossiness
        }

        // 3. Conversion to PBR Standard
        F0 = specular_color;
        roughness = sqrt(max(1.0 - glossiness, 0.001));
        alpha = roughness * roughness;
        metallic = 0.0; // SpecGloss doesn't use metallic parameter usually, diffuse is explicitly provided
    } 
    // WORKFLOW B: METALLIC - ROUGHNESS (Standard)
    else {
        vec4 base_color = mat.base_color_factor * vec4(vertex_color, 1.0);
        if (mat.albedo_id > 0) {
            base_color *= sampleTexture(mat.albedo_id, tex_coord);
        }
        albedo = base_color.rgb;
        alpha = base_color.a;

        metallic = mat.metallic_factor;
        roughness = mat.roughness_factor;
        
        if (mat.mr_id > 0) {
            vec4 mr_sample = sampleTexture(mat.mr_id, tex_coord);
            metallic *= mr_sample.b; // glTF is Blue for Metallic
            roughness *= mr_sample.g; // glTF is Green for Roughness
        }

        // Calculate F0 based on Dielectric/Conductor logic
        F0 = mix(vec3(0.04), albedo, metallic);
        albedo = albedo * (1.0 - metallic); // Darken albedo for metals
    }

    // --- 4. COMMON MATERIAL PROPERTIES ---
    // Clearcoat
    float clearcoat = mat.clearcoat_factor;
    float cc_roughness = mat.clearcoat_roughness_factor; 
    if (mat.clearcoat_id > 0) { 
        clearcoat *= sampleTexture(mat.clearcoat_id, tex_coord).r;
    }
    if (mat.clearcoat_roughness_id > 0) {
        cc_roughness *= sampleTexture(mat.clearcoat_roughness_id, tex_coord).r;
    }

    // Occlusion
    float ao = mat.occlusion_strength; 
    if (mat.occlusion_id > 0) ao *= sampleTexture(mat.occlusion_id, tex_coord).r;
    
    // Emissive
    vec3 emissive = mat.emissive_factor_and_pad.xyz;
    if (mat.emissive_id > 0){ 
        emissive *= sampleTexture(mat.emissive_id, tex_coord).rgb;
    }

    // --- 5. TRANSPARENCY & MASK LOGIC ---
    float real_alpha_cutoff = mat.alpha_cutoff;
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
    else if (mat.pad > 0.5 && alpha < 0.95) {
        if (alpha < 0.15 && transmission == 0.0) { 
             payload.hit_flag = 2.0;
             payload.color = vec3(0.0); 
             payload.weight = vec3(1.0);
             payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001;
             payload.next_ray_dir = gl_WorldRayDirectionEXT;
             return;
        } 
        transmission = max(transmission, 1.0 - alpha);
    } 
    // CASE C: OPAQUE
    else {
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

    // --- 6. LIGHTING ---
    payload.hit_pos = hit_pos;
    payload.normal = N;
    
    vec3 Lo = vec3(0.0);
    if (transmission == 0.0) {
        if(mat.use_specular_glossiness_workflow > 0.0){
            sampleLights_SG(hit_pos, N, V, albedo, roughness, F0, transmission, Lo);
        }
        else{
            sampleLights(hit_pos, N, V, albedo, roughness, metallic, F0, transmission, Lo);
        }
    }

    vec3 final_emissive = vec3(0.0);
    if (payload.is_specular) {
        final_emissive = emissive;
    }
    payload.color = Lo + final_emissive;

    // --- 7. NEXT RAY GENERATION ---
    payload.hit_flag = 2.0;

    // GLASS PATH
    if (transmission > 0.0) {
        payload.next_ray_origin = hit_pos - N_geo * 0.001;
        vec3 F_val = F_Schlick(abs(dot(N, V)), F0); 
        float prob_reflect = max(max(F_val.r, F_val.g), F_val.b);
        if (rnd(payload.seed) < prob_reflect) { 
            payload.next_ray_origin = hit_pos + N_geo * 0.001;
            payload.next_ray_dir = reflect(-V, N); 
            payload.weight = vec3(1.0);
        } else {
            float refraction_ior = 1.01;
            float eta = (dot(N_geo, V) > 0.0) ? (1.0 / refraction_ior) : refraction_ior;
            vec3 N_refr = (dot(N_geo, V) > 0.0) ? N_geo : -N_geo; 
            
            vec3 refractDir = refract(-V, N_refr, eta);
            if (length(refractDir) > 0.0) { 
                payload.next_ray_dir = refractDir;
                // Use unified albedo for weight instead of base_color_factor from mat
                payload.weight = albedo; 
            } else {
                payload.next_ray_origin = hit_pos + N_geo * 0.001;
                payload.next_ray_dir = reflect(-V, N); 
                payload.weight = vec3(1.0);
            }
        }
        payload.is_specular = true;
        return;
    } 

    // OPAQUE PATH
    payload.next_ray_origin = hit_pos + N_geo * 0.001;
    
    // Valid Clearcoat Logic
    bool cc_valid = (clearcoat > 0.0); 
    vec3 H_cc = sampleGGX(N, cc_roughness, payload.seed);
    vec3 L_cc = reflect(-V, H_cc);
    
    if (dot(L_cc, N_geo) <= 0.0) cc_valid = false; 

    float prob_cc = 0.0;
    vec3 F_cc = vec3(0.0); 

    if (cc_valid) {
        F_cc = F_Schlick(abs(dot(H_cc, V)), vec3(0.04));
        prob_cc = max(F_cc.r, max(F_cc.g, F_cc.b)) * clearcoat;
        prob_cc = clamp(prob_cc, 0.0, 1.0); 
    }

    if (cc_valid && rnd(payload.seed) < prob_cc) {
        // --- HIT CLEARCOAT ---
        payload.next_ray_dir = L_cc;
        payload.weight = vec3(1.0);
        payload.is_specular = true;
    } 
    else {
        // --- HIT BASE LAYER ---
        vec3 energy_attenuation = vec3(1.0);
        if (cc_valid) { 
             float prob_base = max(1.0 - prob_cc, 0.001);
             energy_attenuation = (1 - clearcoat) * (1 - F_cc);
        } 

        float prob_specular = mix(0.04, 1.0, metallic);
        prob_specular = mix(prob_specular, 1.0, pow(1.0 - max(dot(N, V), 0.0), 5.0)); 
        prob_specular = clamp(prob_specular, 0.05, 0.95);
        
        if (rnd(payload.seed) < prob_specular) { 
            // Specular Reflection
            vec3 H = sampleGGX(N, roughness, payload.seed);
            vec3 L = reflect(-V, H);
            
            if (dot(L, N_geo) <= 0.0) {
                payload.weight = vec3(0.0);
            } else {
                payload.next_ray_dir = L;
                vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
                payload.weight = (F * (1.0 / prob_specular)) * energy_attenuation;
            }
            payload.is_specular = true;
        } else {
            // Diffuse Reflection
            vec3 L = sampleCosineHemisphere(N, payload.seed);
            if (dot(L, N_geo) <= 0.0) payload.weight = vec3(0.0);
            
            payload.next_ray_dir = L;
            // Use unified albedo
            // Note: For SpecGloss workflow, metallic is 0, so diffuseColor = albedo.
            // For MetalRough, albedo has already been darkened by (1-metallic) in the branching block.
            // However, to avoid double darkening:
            // In the MetalRough branch, I did: albedo = albedo * (1.0 - metallic).
            // So here, I should NOT multiply again. 
            // Let's correct this line to just use 'albedo' as it is fully prepared.
            
            // Correction:
            payload.weight = (albedo * (1.0 / (1.0 - prob_specular))) * energy_attenuation;
            payload.is_specular = false;
        }
    }
}