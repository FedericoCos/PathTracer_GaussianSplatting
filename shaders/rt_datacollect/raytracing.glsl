#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// --- 1. GEOMETRY INPUTS ---
struct InputVertex {
    vec3 pos;
    vec3 normal;
    vec3 color;
    vec4 tangent;
    vec2 tex_coord;
    vec2 tex_coord_1;
};

// --- 2. MATERIAL DATA ---
struct MaterialData {
    vec4  base_color_factor;
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
};

struct PointLight {
    vec4 position;
    vec4 color;
};

struct MeshInfo {
    uint material_index;
    uint vertex_offset;
    uint index_offset;
    uint _pad1;
};

struct RaySample {
    vec2 uv;
};

// --- 3. OUTPUT DATA ---
struct HitData {
    vec3 hit_pos;
    float hit_flag; 
    vec4 color;
    vec3 normal;
    float padding;
};

// --- 4. PAYLOADS (THE FIX) ---
struct RayPayload {
    // A. Lighting Result
    vec3 color;
    
    // B. Next Ray State (For Iterative Loop)
    vec3 next_ray_origin;
    vec3 next_ray_dir;
    float hit_flag; // -1=Miss, 1=Stop, 2=Bounce
    vec3 weight;    // Color attenuation for next bounce
    
    // C. Geometry Capture (For Point Cloud)
    vec3 hit_pos;
    vec3 normal;
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
    vec4 ambientLight;
    PointLight pointlights[150];
    PointLight shadowLights[150];
    int cur_num_pointlights;
    int cur_num_shadowlights;
    int panelShadowsEnabled;
    float shadowFarPlane;
} ubo;
layout(set = 0, binding = 5, scalar) buffer readonly AllVertices { InputVertex v[]; } all_vertices;
layout(set = 0, binding = 6, scalar) buffer readonly AllIndices { uint i[]; } all_indices;
layout(set = 0, binding = 7, scalar) buffer readonly AllMeshInfo { MeshInfo info[]; } all_mesh_info;

// Binding 9: Output Image
layout(set = 0, binding = 9, rgba8) uniform image2D rt_output_image;
// Binding 10: Textures
layout(set = 0, binding = 10) uniform sampler2D global_textures[];

#endif