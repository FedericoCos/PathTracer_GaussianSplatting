#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_tracing : require

// Struct for our input vertices (matches Vertex struct)
struct InputVertex {
    vec3 pos;
    vec3 normal;
    vec3 color;
    vec4 tangent;
    vec2 tex_coord;
    vec2 tex_coord_1;
};

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
    vec2 uv; // we use u for major angle, v for minor angle
};

// Our output data structure
struct HitData {
    vec3 hit_pos;
    float hit_flag; // 1.0 = hit, -1.0 = miss
    vec4 color;
};

// Payload passed from rgen -> rchit/rmiss
struct RayPayload {
    uint vertex_index;
    vec3 color;
};

// Binding 3: All Materials
layout(set = 0, binding = 3, scalar) buffer readonly AllMaterialsBuffer {
   MaterialData materials[];
} all_materials;

// Binding 5: All Vertices
layout(set = 0, binding = 5, scalar) buffer readonly AllVertices {
   InputVertex v[];
} all_vertices;

// Binding 6: All Indices
layout(set = 0, binding = 6, scalar) buffer readonly AllIndices {
   uint i[];
} all_indices;

// Binding 7: All MeshInfo
layout(set = 0, binding = 7, scalar) buffer readonly AllMeshInfo {
   MeshInfo info[];
} all_mesh_info;


// Binding 8: Global Texture Array (Unbounded)
layout(set = 0, binding = 9) uniform sampler2D global_textures[];