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