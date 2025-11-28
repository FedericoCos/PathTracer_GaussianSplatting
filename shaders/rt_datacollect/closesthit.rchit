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
    vec3 originOffset = origin + L * 0.01;
    shadowPayload.isHit = true; 
    
    // CURRENT FLAGS:
    // gl_RayFlagsOpaqueEXT -> Treats EVERYTHING as opaque. Stops at glass.
    
    // THE FIX IS COMPLEX without C++ AnyHit shaders.
    // For now, stick with the "Shadow Ignore Factor" logic I gave you in the previous message.
    // Ensure this logic is present in your main() function:
    
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, originOffset, 0.0, L, dist, 1);
    
    if (shadowPayload.isHit) return 0.0;
    return 1.0;
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

    if (alpha < mat.alpha_cutoff) {
        payload.hit_flag = -1.0; 
        payload.color = vec3(0.0);
        return; 
    }

    // --- DETERMINE MATERIAL TYPE (MOVED UP) ---
    float transmission = mat.transmission_factor; 
    bool is_glass = (transmission > 0.001);
    bool is_alpha_transparent = (alpha < 0.999 && !is_glass);

    // Calculate how much we ignore shadows. 
    // If fully transparent (1.0), we ignore shadows (1.0).
    float shadow_ignore_factor = is_glass ? transmission : (1.0 - alpha);

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
    for(int i = 0; i < ubo.cur_num_pointlights; i++) {
        vec3 L = normalize(ubo.pointlights[i].position.xyz - hit_pos);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float dist = length(ubo.pointlights[i].position.xyz - hit_pos);
        float atten = 1.0 / (dist * dist);
        vec3 radiance = ubo.pointlights[i].color.rgb * ubo.pointlights[i].color.a * atten;
        
        float shadow = traceShadow(hit_pos, ubo.pointlights[i].position.xyz);
        
        // FIX: If object is transparent, blend shadow towards 1.0 (unshadowed)
        shadow = max(shadow, shadow_ignore_factor);

        float NDF = D_GGX(N, H, roughness);
        float Vis = V_SmithGGXCorrelatedFast(NdotV, NdotL, roughness);
        vec3 F = F_Schlick(dot(H, V), F0);
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        vec3 specular = NDF * Vis * F;
        
        // Scale diffuse by opaque factor so we don't double-count light (surface + transmit)
        vec3 diffuse = (kD * albedo / PI) * (1.0 - shadow_ignore_factor);
        
        Lo += (diffuse + specular) * radiance * NdotL * shadow;
    }
    // (You can apply the same shadow logic to shadowLights loop if needed)

    vec3 ambient = ubo.ambientLight.xyz * ubo.ambientLight.w * albedo * ao * (1.0 - shadow_ignore_factor);
    payload.color = ambient + Lo + emissive;

    // --- NEXT RAY SETUP ---
    payload.hit_flag = 1.0; 
    payload.weight = vec3(0.0);

    if (is_alpha_transparent) {
        payload.next_ray_origin = hit_pos + gl_WorldRayDirectionEXT * 0.001; 
        payload.next_ray_dir = gl_WorldRayDirectionEXT;
        payload.weight = vec3(1.0 - alpha); 
        payload.hit_flag = 2.0; 
    }
    else if (is_glass) {
        float eta = is_back_face ? (1.5 / 1.0) : (1.0 / 1.5);
        vec3 N_refr = is_back_face ? -N : N; 
        vec3 refractDir = refract(gl_WorldRayDirectionEXT, N_refr, eta);

        if (length(refractDir) > 0.0) {
            payload.next_ray_origin = hit_pos - N_geo * 0.001; 
            payload.next_ray_dir = refractDir;
            payload.weight = vec3(transmission); 
            payload.hit_flag = 2.0; 
        } else {
            // TIR
            vec3 reflectDir = reflect(-V, N);
            payload.next_ray_origin = hit_pos + N_geo * 0.001;
            payload.next_ray_dir = reflectDir;
            payload.weight = vec3(transmission); 
            payload.hit_flag = 2.0; 
        }
    }
    else if (metallic > 0.1 || roughness < 0.2) {
        vec3 reflectDir = reflect(-V, N);
        vec3 F = F_Schlick(NdotV, F0);
        payload.next_ray_origin = hit_pos + N_geo * 0.001;
        payload.next_ray_dir = reflectDir;
        payload.weight = F * (1.0 - roughness); 
        payload.hit_flag = 2.0; 
    }
}