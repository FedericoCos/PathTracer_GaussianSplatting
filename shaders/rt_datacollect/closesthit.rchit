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
const float PHI = 1.61803398875;

float getBlueNoise(int dimension) {
    float base = (dimension % 2 == 0) ? payload.blue_noise.x : payload.blue_noise.y;
    return fract(base + (float(payload.depth) * PHI) + (float(dimension) * 0.754877));
}

float computeLOD(vec3 origin, vec3 dir, float dist, vec3 normal, int tex_id) {
    const float pixel_spread_angle = (2.0 * tan(ubo.fov * 0.5)) / ubo.win_height;
    float ray_footprint = dist * pixel_spread_angle;
    float NdotV = abs(dot(normal, -dir));
    ray_footprint /= max(NdotV, 0.25);

    float texture_dim = 2048.0;
    if (tex_id >= 0) {
        ivec2 size = textureSize(global_textures[nonuniformEXT(tex_id)], 0);
        texture_dim = float(max(size.x, size.y));
    }

    float raw_lod = log2(ray_footprint * texture_dim);
    float bias = (texture_dim > 2048.0) ? 0.0 : -0.5;
    float final_lod = (raw_lod * 0.7) + bias;
    return max(final_lod, 0.0);
}

vec4 sampleTexture(int texture_id, vec2 uv, float lod) {
    if (texture_id < 0) return vec4(1.0);
    return textureLod(global_textures[nonuniformEXT(texture_id)], uv, lod);
}

vec3 safeNormalize(vec3 v) {
    float len = length(v);
    return (len < 1e-6) ? vec3(0.0, 1.0, 0.0) : v / len;
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

vec3 sampleCosineHemisphere(vec3 N) {
    float r1 = getBlueNoise(0);
    float r2 = getBlueNoise(1);
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
    float a = roughness * roughness;
    float ggxV = NdotL * (NdotV * (1.0 - a) + a);
    float ggxL = NdotV * (NdotL * (1.0 - a) + a);
    return 0.5 / max(ggxV + ggxL, 0.0001);
}

vec3 sampleGGX(vec3 N, float roughness) {
    float r1 = getBlueNoise(2);
    float r2 = getBlueNoise(3);
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

float pdf_GGX(vec3 N, vec3 V, vec3 L, float roughness) {
    vec3 H = safeNormalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float D_NdotH = D_GGX(N, H, roughness) * NdotH;
    return D_NdotH / (4.0 * VdotH + 0.0001);
}

float pdf_Lambert(vec3 N, vec3 L) {
    return max(dot(N, L), 0.0) / PI;
}

float traceShadowDist(vec3 origin, vec3 direction, float maxDist) {
    shadowPayload.isHit = true;
    traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0,0,1, origin, 0.001, direction, maxDist, 1);
    return shadowPayload.isHit ? 0.0 : 1.0;
}

float traceShadow(vec3 origin, vec3 lightPos) {
    vec3 L = lightPos - origin;
    float dist = length(L); 
    L = safeNormalize(L);
    shadowPayload.isHit = true;
    traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, origin, 0.001, L, dist - 0.005, 1);
    return shadowPayload.isHit ? 0.0 : 1.0;
}

void samplePunctualLights(vec3 hit_pos, vec3 N, vec3 N_geo, vec3 V, vec3 albedo, float roughness, vec3 F0, float transmission, inout vec3 Lo) {
    uint num_lights = scene_lights.lights.length();
    float r_select = getBlueNoise(4);
    uint light_idx = 0;
    uint left = 0, right = num_lights;
    while (left < right) {
        uint mid = (left + right) >> 1;
        if (punctual_cdf.entries[mid].cumulative_probability < r_select) left = mid + 1;
        else { light_idx = mid; right = mid; }
    }
    
    PunctualLight light = scene_lights.lights[light_idx];
    vec3 L;
    float dist;
    float attenuation = 1.0;
    
    if (light.type == 1) { // Directional
        L = normalize(-light.direction);
        dist = 10000.0;
        attenuation = 1.0;
    } else { // Point/Spot
        vec3 offset = light.position - hit_pos;
        float dist_sq = dot(offset, offset);
        dist_sq = max(dist_sq, 0.01); 
        dist = sqrt(dist_sq);
        L = offset / dist;
        attenuation = 1.0 / dist_sq;
        if (light.range > 0.0) {
            float range_atten = max(min(1.0 - pow(dist / light.range, 4.0), 1.0), 0.0) / (dist_sq);
            attenuation = range_atten / dist_sq;
        }
        if (light.type == 2) {
            float cos_dir = dot(-L, normalize(light.direction));
            float spot_scale = 1.0 / max(light.inner_cone_cos - light.outer_cone_cos, 0.001);
            float spot_offset = -light.outer_cone_cos * spot_scale;
            float spot_atten = clamp(cos_dir * spot_scale + spot_offset, 0.0, 1.0);
            attenuation *= spot_atten * spot_atten;
        }
    }
    
    vec3 Le = light.color * light.intensity * attenuation;
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL < 0.001) return;
    
    if (NdotL > 0.0 && length(Le) > 0.0) {
        vec3 shadow_origin = hit_pos + N_geo * 0.001;
        float visibility;
        if (light.type == 1) visibility = traceShadowDist(shadow_origin, L, 10000.0);
        else visibility = traceShadow(shadow_origin, light.position);

        visibility = max(visibility, transmission);
        if (visibility > 0.0) {
            float weight = float(num_lights);
            vec3 H = safeNormalize(V + L);
            float NDF = D_GGX(N, H, roughness);
            float Vis = V_SmithGGXCorrelatedFast(dot(N,V), NdotL, roughness);
            vec3 F = F_Schlick(dot(H, V), F0);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - transmission);
            vec3 specular = NDF * Vis * F;
            vec3 diffuse = (kD * albedo / PI) * (1.0 - transmission);
            Lo += (diffuse + specular) * Le * NdotL * visibility * weight;
        }
    }
}

void sampleLights_SG(vec3 hit_pos, vec3 N, vec3 N_geo, vec3 V, vec3 albedo, float roughness, vec3 F0, float transmission, inout vec3 Lo) {
    uint num_lights = light_cdf.entries.length();
    float r_select = getBlueNoise(4);
    uint idx = 0;
    uint left = 0, right = num_lights;
    while (left < right) {
        uint mid = (left + right) >> 1;
        if (light_cdf.entries[mid].cumulative_probability < r_select) left = mid + 1;
        else { idx = mid; right = mid; }
    }

    uint tri_idx = light_cdf.entries[idx].triangle_index;
    LightTriangle tri = light_triangles.tris[tri_idx];
    vec3 p0 = all_vertices.v[tri.v0].pos;
    vec3 p1 = all_vertices.v[tri.v1].pos;
    vec3 p2 = all_vertices.v[tri.v2].pos;

    float u = getBlueNoise(5); 
    float v = getBlueNoise(6);
    if (u + v > 1.0) { u = 1.0 - u; v = 1.0 - v; }
    vec3 light_pos = p0 * (1.0 - u - v) + p1 * u + p2 * v;
    vec3 light_normal = normalize(cross(p1 - p0, p2 - p0));

    vec3 L = light_pos - hit_pos;
    float dist_sq = dot(L, L);
    dist_sq = max(dist_sq, 0.0001); 
    float dist = sqrt(dist_sq);
    L /= dist;

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL < 0.001) return;
    float LdotN_light = abs(dot(-L, light_normal));
    if (NdotL > 0.0 && LdotN_light > 0.0) {
        vec3 shadow_origin = hit_pos + N_geo * 0.001;
        float visibility = traceShadow(shadow_origin, light_pos);
        visibility = max(visibility, transmission);

        if (visibility > 0.0) {
            MaterialData l_mat = all_materials.materials[tri.material_index];
            vec3 Le = l_mat.emissive_factor_and_pad.rgb;
            float emission_strength = max(Le.r, max(Le.g, Le.b));
            float pdf_nee = (emission_strength / ubo.emissive_flux) * (dist_sq / LdotN_light);
            float pdf_spec = pdf_GGX(N, V, L, roughness);
            float pdf_diff = pdf_Lambert(N, L);
            // Match the actual sampling strategy used in the BRDF path
            float prob_specular = clamp(length(F0), 0.05, 0.95);
            float prob_diffuse = 1.0 - prob_specular;
            float pdf_bsdf = (pdf_spec * prob_specular) + (pdf_diff * prob_diffuse);
            float mis_weight = (pdf_nee * pdf_nee) / (pdf_nee * pdf_nee + pdf_bsdf * pdf_bsdf);

            vec3 H = safeNormalize(V + L);
            float NDF = D_GGX(N, H, roughness);
            float Vis = V_SmithGGXCorrelatedFast(dot(N,V), NdotL, roughness);
            vec3 F = F_Schlick(dot(H, V), F0);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS);
            vec3 specular = NDF * Vis * F;
            vec3 diffuse = (kD * albedo / PI) * (1.0 - transmission);
            vec3 brdf = diffuse + specular;
            if(pdf_nee > 1e-10)
                Lo += brdf * Le * NdotL * (1.0 / pdf_nee) * mis_weight * visibility * ubo.ambientLight.w;
        }
    }
}

void sampleLights(vec3 hit_pos, vec3 N, vec3 N_geo, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0, float transmission, inout vec3 Lo) {
    uint num_lights = light_cdf.entries.length();
    float r_select = getBlueNoise(7);
    uint idx = 0;
    uint left = 0, right = num_lights;
    while (left < right) {
        uint mid = (left + right) >> 1;
        if (light_cdf.entries[mid].cumulative_probability < r_select) left = mid + 1;
        else { idx = mid; right = mid; }
    }
    
    uint tri_idx = light_cdf.entries[idx].triangle_index;
    LightTriangle tri = light_triangles.tris[tri_idx];
    vec3 p0 = all_vertices.v[tri.v0].pos;
    vec3 p1 = all_vertices.v[tri.v1].pos;
    vec3 p2 = all_vertices.v[tri.v2].pos;

    float u = getBlueNoise(8); 
    float v = getBlueNoise(9);
    if (u + v > 1.0) { u = 1.0 - u; v = 1.0 - v; }
    vec3 light_pos = p0 * (1.0 - u - v) + p1 * u + p2 * v;
    vec3 light_normal = safeNormalize(cross(p1 - p0, p2 - p0));
    
    vec3 L = light_pos - hit_pos;
    float dist_sq = dot(L, L);
    dist_sq = max(dist_sq, 0.0001); 
    float dist = sqrt(dist_sq);
    L /= dist;
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL < 0.001) return;
    float LdotN_light = abs(dot(-L, light_normal));
    if (NdotL > 0.0 && LdotN_light > 0.0) {
        vec3 shadow_origin = hit_pos + N_geo * 0.001;
        float visibility = traceShadow(shadow_origin, light_pos);
        visibility = max(visibility, transmission);
        if (visibility > 0.0) {
            MaterialData l_mat = all_materials.materials[tri.material_index];
            vec3 Le = l_mat.emissive_factor_and_pad.rgb;
            float emission_strength = max(Le.r, max(Le.g, Le.b));
            
            float pdf_nee = (emission_strength / ubo.emissive_flux) * (dist_sq / LdotN_light);
            float prob_specular = mix(0.04, 1.0, metallic);
            float pdf_spec = pdf_GGX(N, V, L, roughness);
            float pdf_diff = pdf_Lambert(N, L);
            float prob_diffuse = 1.0 - prob_specular;
            float pdf_bsdf = (pdf_spec * prob_specular) + (pdf_diff * prob_diffuse);
            float mis_weight = (pdf_nee * pdf_nee) / (pdf_nee * pdf_nee + pdf_bsdf * pdf_bsdf);

            vec3 H = safeNormalize(V + L);
            float NDF = D_GGX(N, H, roughness);
            float Vis = V_SmithGGXCorrelatedFast(dot(N,V), NdotL, roughness);
            vec3 F = F_Schlick(dot(H, V), F0);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS);
            vec3 specular = NDF * Vis * F;
            vec3 diffuse = (kD * albedo / PI) * (1.0 - transmission);
            vec3 brdf = diffuse + specular;
            if(pdf_nee > 1e-10)
                Lo += brdf * Le * NdotL * (1.0 / pdf_nee) * mis_weight * visibility * ubo.ambientLight.w;
        }
    }
}

hitAttributeEXT vec2 hitAttrib;

void main()
{
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
    vec3 N_geo = safeNormalize(vec3(gl_ObjectToWorldEXT * vec4(normal_obj, 0.0)));
    vec3 T_geo = safeNormalize(vec3(gl_ObjectToWorldEXT * vec4(tangent_obj.xyz, 0.0))); 
    T_geo = safeNormalize(T_geo - dot(T_geo, N_geo) * N_geo);
    vec3 B_geo = cross(N_geo, T_geo) * sign(tangent_obj.w); 

    vec3 V = -gl_WorldRayDirectionEXT;
    vec3 N_geo_original = N_geo;  // Preserve original for glass
    if (dot(N_geo, V) < 0.0) N_geo = -N_geo;
    vec3 N = N_geo;
    vec3 T = T_geo; 
    
    float Tw = tangent_obj.w;
    bool tangentValid = (abs(Tw) > 0.001) && (length(T) > 0.001);
    if (!tangentValid) {
        vec3 up = (abs(N.y) < 0.999) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        T = safeNormalize(cross(up, N));
    }
    float handedness = (Tw < 0.0) ? -1.0 : 1.0;
    vec3 B = safeNormalize(cross(N, T)) * handedness;
    mat3 TBN = mat3(T, B, N);

    if (abs(Tw) > 0.0001 && mat.normal_id > 0) {
        float tex_lod = 0.0;
        if(ubo.use_lod > 0.0){
            if (payload.last_bsdf_pdf <= 0.0) {
                if (mat.albedo_id > 0) tex_lod = computeLOD(gl_WorldRayOriginEXT, gl_WorldRayDirectionEXT, gl_HitTEXT, N_geo, mat.albedo_id);
            } else {
                tex_lod = clamp(mat.roughness_factor * 5.0 + log2(gl_HitTEXT * 0.1 + 1.0), 0.0, 8.0);
            }
            vec3 map_val = sampleTexture(mat.normal_id, (mat.uv_normal * vec4(tex_coord, 0.0, 1.0)).xy, tex_lod).xyz;
            vec3 normal_map = map_val * 2.0 - 1.0;
            normal_map.xy *= ubo.lod_factor;
            normal_map = normalize(normal_map);
            N = safeNormalize(TBN * normal_map);
        }
        else{
            vec3 map_val = sampleTexture(mat.normal_id, (mat.uv_normal * vec4(tex_coord, 0.0, 1.0)).xy, 0).xyz;
            vec3 normal_map = map_val * 2.0 - 1.0;
            normal_map.xy *= ubo.lod_factor;
            normal_map = normalize(normal_map);
            N = safeNormalize(TBN * normal_map);
        }
    }

    vec3 albedo;
    float alpha;
    vec3 F0;
    float roughness;
    float metallic;

    vec4 base_color_sample = vec4(1.0);
    if (mat.albedo_id > 0) base_color_sample = sampleTexture(mat.albedo_id, tex_coord, 0);

    if (mat.use_specular_glossiness_workflow > 0.5) {
        vec4 diffuse_sample = mat.base_color_factor * vec4(vertex_color, 1.0) * base_color_sample;
        albedo = diffuse_sample.rgb;
        alpha  = diffuse_sample.a;
        vec3 specular_color = mat.specular_color_factor;
        float glossiness = mat.roughness_factor; 
        if (mat.sg_id > 0) {
            vec4 sg_tex = sampleTexture(mat.sg_id, tex_coord, 0);
            specular_color *= sg_tex.rgb; 
            glossiness *= sg_tex.a;
        }
        F0 = specular_color;
        roughness = sqrt(max(1.0 - glossiness, 0.04));
        metallic = 0.0;
    } 
    else {
        vec4 base_color = mat.base_color_factor * vec4(vertex_color, 1.0) * base_color_sample;
        albedo = base_color.rgb;
        alpha = base_color.a;
        metallic = mat.metallic_factor;
        roughness = mat.roughness_factor;
        if (mat.mr_id > 0) {
            vec4 mr_sample = sampleTexture(mat.mr_id, tex_coord, 0);
            metallic *= mr_sample.b; 
            roughness *= mr_sample.g;
        }
        F0 = mix(vec3(0.04), albedo, metallic);
        albedo = albedo * (1.0 - metallic); 
    }

    float clearcoat = mat.clearcoat_factor;
    float cc_roughness = mat.clearcoat_roughness_factor; 
    // OPTIMIZATION: Conditional Fetch
    if (clearcoat > 0.0 && mat.clearcoat_id > 0) clearcoat *= sampleTexture(mat.clearcoat_id, tex_coord, 0).r;
    if (clearcoat > 0.0 && mat.clearcoat_roughness_id > 0) cc_roughness *= sampleTexture(mat.clearcoat_roughness_id, tex_coord, 0).r;

    float ao = mat.occlusion_strength;
    // OPTIMIZATION: Conditional Fetch
    if (ao > 0.0 && mat.occlusion_id > 0) ao *= sampleTexture(mat.occlusion_id, tex_coord, 0).r;
    
    vec3 emissive = mat.emissive_factor_and_pad.xyz;
    // OPTIMIZATION: Conditional Fetch
    if (length(emissive) > 0.0 && mat.emissive_id > 0){ 
        emissive *= sampleTexture(mat.emissive_id, (mat.uv_emissive * vec4(tex_coord, 0.0, 1.0)).xy, 0).rgb;
    }
    float transmission = mat.transmission_factor; 

    // --- LIGHTING ---
    // REVERTED: Writing hit_pos and normal to payload as requested
    payload.hit_pos = hit_pos;
    payload.normal = N;
    
    vec3 Lo = vec3(0.0);
    vec3 Le = emissive;
    float emission_strength = length(emissive);
    float nee_treshold = 0.001;
    bool use_nee = (transmission == 0.0) && (roughness > nee_treshold);

    if (payload.hit_flag > 3.0) use_nee = false;
    if (emission_strength > 0.0) {
        if (payload.last_bsdf_pdf <= 0.0 || !use_nee) Lo += Le;
        else if(emission_strength < 0.001) { payload.color = vec3(0.0); return; }
        else {
            float pdf_bsdf = payload.last_bsdf_pdf;
            float emission_strength_cpu = length(mat.emissive_factor_and_pad.rgb);
            vec3 L_to_cam = -gl_WorldRayDirectionEXT;
            float LdotN = abs(dot(N, L_to_cam));
            float dist_sq = gl_HitTEXT * gl_HitTEXT;
            float pdf_nee = 0.0;
            if (ubo.emissive_flux > 0.0) pdf_nee = (emission_strength_cpu / (ubo.emissive_flux)) * (dist_sq / max(LdotN, 0.001));
            float prob_nee_strategy = (ubo.punctual_flux > 0.0) ? ubo.p_emissive : 1.0;
            pdf_nee *= prob_nee_strategy;
            float mis_weight = (pdf_bsdf * pdf_bsdf) / (pdf_bsdf * pdf_bsdf + pdf_nee * pdf_nee);
             Lo += Le * mis_weight;
        }
    }

    bool has_emissive = ubo.emissive_flux > 0;
    bool has_punctual = ubo.punctual_flux > 0;
    vec3 light_contr = vec3(0.0);

    if (has_emissive && has_punctual) {
        float p_punctual = 1.0 - ubo.p_emissive;
        if (getBlueNoise(10) < ubo.p_emissive) {
            if(use_nee){
                if(mat.use_specular_glossiness_workflow > 0.0) sampleLights_SG(hit_pos, N, N_geo, V, albedo, roughness, F0, transmission, light_contr);
                else sampleLights(hit_pos, N, N_geo, V, albedo, roughness, metallic, F0, transmission, light_contr);
                light_contr *= (1.0 / ubo.p_emissive);
            }
        } else {
            samplePunctualLights(hit_pos, N, N_geo, V, albedo, roughness, F0, transmission, light_contr);
            light_contr *= (1.0 / p_punctual);
        }
        Lo += light_contr;
    } 
    else if (has_emissive && use_nee) {
        if(mat.use_specular_glossiness_workflow > 0.0) sampleLights_SG(hit_pos, N, N_geo, V, albedo, roughness, F0, transmission, Lo);
        else sampleLights(hit_pos, N, N_geo, V, albedo, roughness, metallic, F0, transmission, Lo);
    }
    else if (has_punctual) {
        samplePunctualLights(hit_pos, N, N_geo, V, albedo, roughness, F0, transmission, Lo);
    }

    payload.color = Lo;

    // --- NEXT RAY GENERATION ---
    // OPTIMIZATION: Set specific flag for Opaque (1.0) vs Glass (2.0)
    
    if (transmission > 0.0) {
        payload.hit_flag = 2.0; // GLASS
        bool entering = dot(N_geo_original, V) > 0.0;
        vec3 N_refr = entering ? N_geo_original : -N_geo_original;
        
        payload.next_ray_origin = hit_pos - N_refr * 0.001;
        vec3 F_val = F_Schlick(abs(dot(N_geo_original, V)), F0); 
        float prob_reflect = max(max(F_val.r, F_val.g), F_val.b);
        
        if (getBlueNoise(11) < prob_reflect) { 
            payload.next_ray_origin = hit_pos + N_refr * 0.001;
            payload.next_ray_dir = reflect(-V, N_refr); 
            payload.weight = vec3(1.0);
        } else {
            float refraction_ior = 1.01;
            float eta = entering ? (1.0 / refraction_ior) : refraction_ior;
            vec3 refractDir = refract(-V, N_refr, eta);
            if (length(refractDir) > 0.0) { 
                payload.next_ray_dir = refractDir;
                payload.weight = albedo;
            } else {
                payload.next_ray_origin = hit_pos + N_geo * 0.001;
                payload.next_ray_dir = reflect(-V, N); 
                payload.weight = vec3(1.0);
            }
        }
        payload.last_bsdf_pdf = 0.0;
        return;
    } 

    // OPAQUE PATH
    payload.hit_flag = 1.0; // OPAQUE
    payload.next_ray_origin = hit_pos + N_geo * 0.001; 

    float NdotV = max(dot(N, V), 0.0);
    float cc_prob = 0.0;
    vec3 F_cc_view = vec3(0.0);

    if (clearcoat > 0.0) {
        F_cc_view = F_Schlick(NdotV, vec3(0.04)) * clearcoat;
        cc_prob = clamp(max(F_cc_view.r, max(F_cc_view.g, F_cc_view.b)), 0.0, 1.0);
    }

    if (clearcoat > 0.0 && getBlueNoise(12) < cc_prob) {
        // Clearcoat path
        vec3 H_cc = sampleGGX(N, cc_roughness);
        vec3 L_cc = reflect(-V, H_cc);
        float NdotL = max(dot(N, L_cc), 0.0);
        float NdotH = max(dot(N, H_cc), 0.0);
        float VdotH = max(dot(V, H_cc), 0.0);

        if (dot(L_cc, N_geo) <= 0.0) {
            payload.weight = vec3(0.0);
            payload.last_bsdf_pdf = 0.0;
        } else {
            payload.next_ray_dir = L_cc;
            vec3 F = F_Schlick(VdotH, vec3(0.04)) * clearcoat;
            float Vis = V_SmithGGXCorrelatedFast(NdotV, NdotL, cc_roughness);
            vec3 specular_weight = F * Vis * 4.0 * NdotL * (VdotH / max(NdotH, 0.0001));
            float pdf_cc = pdf_GGX(N, V, L_cc, cc_roughness);
            
            // Also compute the PDF for the base layer with this direction
            float prob_specular_base = mix(0.04, 1.0, metallic);
            prob_specular_base = mix(prob_specular_base, 1.0, pow(1.0 - NdotV, 5.0));
            prob_specular_base = clamp(prob_specular_base, 0.05, 0.95);
            float prob_diffuse_base = 1.0 - prob_specular_base;
            float pdf_spec_base = pdf_GGX(N, V, L_cc, roughness);
            float pdf_diff_base = pdf_Lambert(N, L_cc);
            float pdf_base = (pdf_spec_base * prob_specular_base) + (pdf_diff_base * prob_diffuse_base);
            
            // Combined PDF accounting for all paths
            payload.last_bsdf_pdf = (pdf_cc * cc_prob) + (pdf_base * (1.0 - cc_prob));
            payload.weight = specular_weight * (1.0 / cc_prob);
        }
    } 
    else {
        vec3 transmission_factor = vec3(1.0) - F_cc_view;
        float selection_weight = 1.0 / (1.0 - cc_prob);
        vec3 energy_attenuation = transmission_factor * selection_weight;
        float prob_specular = mix(0.04, 1.0, metallic);
        prob_specular = mix(prob_specular, 1.0, pow(1.0 - NdotV, 5.0));
        prob_specular = clamp(prob_specular, 0.05, 0.95);
        float prob_diffuse = 1.0 - prob_specular;

        if (getBlueNoise(13) < prob_specular) {
            vec3 H = sampleGGX(N, roughness);
            vec3 L = reflect(-V, H);
            float NdotL = max(dot(N, L), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            if (dot(L, N_geo) <= 0.0) {
                payload.weight = vec3(0.0);
                payload.last_bsdf_pdf = 0.0;
            } else {
                payload.next_ray_dir = L;
                vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
                float Vis = V_SmithGGXCorrelatedFast(NdotV, NdotL, roughness);
                vec3 specular_weight = F * Vis * 4.0 * NdotL * (VdotH / max(NdotH, 0.0001));
                float pdf_spec = pdf_GGX(N, V, L, roughness);
                float pdf_diff = pdf_Lambert(N, L);
                payload.last_bsdf_pdf = ((pdf_spec * prob_specular) + (pdf_diff * prob_diffuse)) * (1.0 - cc_prob);
                payload.weight = (specular_weight * (1.0 / prob_specular)) * energy_attenuation;
            }
        } else {
            vec3 L = sampleCosineHemisphere(N);
            if (dot(L, N_geo) <= 0.0) {
                payload.weight = vec3(0.0);
                payload.last_bsdf_pdf = 0.0;
            } else {
                payload.next_ray_dir = L;
                payload.weight = (albedo * (1.0 / (1.0 - prob_specular))) * energy_attenuation;
                float pdf_spec = pdf_GGX(N, V, L, roughness);
                float pdf_diff = pdf_Lambert(N, L);
                payload.last_bsdf_pdf = ((pdf_spec * prob_specular) + (pdf_diff * prob_diffuse)) * (1.0 - cc_prob);
            }
        }
    }
}