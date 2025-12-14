#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// --- 1. GEOMETRY INPUTS ---
struct InputVertex {
    vec3 pos;
    float pad1;
    vec3 normal;
    float pad2;
    vec3 color;
    float pad3;
    vec4 tangent;
    vec2 tex_coord;
    vec2 tex_coord_1;
};

// --- 2. MATERIAL DATA ---
struct MaterialData {
    vec4  base_color_factor;
    mat4 uv_normal;
    mat4 uv_emissive;
    mat4 uv_albedo;
    vec4  emissive_factor_and_pad;
    float metallic_factor;
    float roughness_factor;
    float occlusion_strength;
    float specular_factor;
    vec3  specular_color_factor;
    float alpha_cutoff;
    float transmission_factor;
    float clearcoat_factor;
    float clearcoat_roughness_factor;
    float pad;
    int albedo_id;
    int normal_id;
    int mr_id;
    int emissive_id;
    int occlusion_id;
    int clearcoat_id;
    int clearcoat_roughness_id;
    int sg_id;
    int use_specular_glossiness_workflow; 
};

struct PunctualLight {
    vec3 position;
    float intensity; 
    vec3 color;
    float range;     
    vec3 direction;
    float outer_cone_cos; 
    float inner_cone_cos;
    int type;
    vec2 padding;
};

// --- NEW: LIGHT DATA STRUCTURES ---
struct LightTriangle {
    uint v0, v1, v2;
    uint material_index;
};

struct LightCDF {
    float cumulative_probability;
    uint triangle_index;
    vec2 padding;
};

struct PunctualCDF {
    float cumulative_probability;
    uint light_index;
    vec2 padding;
};

struct MeshInfo {
    uint material_index;
    uint vertex_offset;
    uint index_offset;
    uint _pad1;
};

// --- 3. OUTPUT DATA ---
// (HitData struct removed as it appeared unused in the provided code)

// --- 4. PAYLOADS ---
// OPTIMIZATION: Removed hit_pos, normal (12+12 = 24 bytes saved)
// Reduced register pressure significantly.
struct RayPayload {
    vec3 color;             // Direct lighting (Le + Direct)
    vec3 next_ray_origin;   
    vec3 next_ray_dir;
    float hit_flag;         // 1.0 = Opaque, 2.0 = Glass/Transmission
    vec3 weight;            // BSDF throughput for next bounce
    uint seed; 
    float last_bsdf_pdf;
    vec2 blue_noise;
    int depth;
};

struct ShadowPayload {
    bool isHit;
};

// --- 5. BINDINGS ---
#ifdef RAY_TRACING

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 3, scalar) buffer readonly AllMaterialsBuffer { MaterialData materials[]; } all_materials;
layout(set = 0, binding = 4) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    uint frameCount;
    vec4 ambientLight; 
    float emissive_flux;
    float punctual_flux;
    float total_flux;
    float p_emissive;
    float fov;
    float win_height;
    vec2 padding;
} ubo;

layout(set = 0, binding = 5, scalar) buffer readonly AllVertices { InputVertex v[]; } all_vertices;
layout(set = 0, binding = 6, scalar) buffer readonly AllIndices { uint i[]; } all_indices;
layout(set = 0, binding = 7, scalar) buffer readonly AllMeshInfo { MeshInfo info[]; } all_mesh_info;

layout(set = 0, binding = 8, scalar) buffer readonly LightTriBuffer { LightTriangle tris[]; } light_triangles;
layout(set = 0, binding = 9, scalar) buffer readonly LightCDFBuffer { LightCDF entries[]; } light_cdf;
layout(set = 0, binding = 10, scalar) buffer readonly PunctualCDFBuffer { PunctualCDF entries[]; } punctual_cdf;

layout(set = 0, binding = 11, rgba32f) uniform image2D rt_output_image;
layout(set = 0, binding = 12) uniform sampler2D blueNoiseTex;
layout(set = 0, binding = 13, scalar) buffer LightBuffer { PunctualLight lights[]; } scene_lights;
layout(set = 0, binding = 14) uniform sampler2D global_textures[];

// --- 6. RANDOM NUMBER GENERATOR ---
float rnd(inout uint state) {
    uint prev = state;
    state = prev * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return float((word >> 22u) ^ word) / 4294967296.0;
}

#endif