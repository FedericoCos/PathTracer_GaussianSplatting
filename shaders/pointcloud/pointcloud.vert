#version 460
#extension GL_EXT_scalar_block_layout : require

// Manually define structs to avoid binding conflicts
struct HitData {
    vec3 hit_pos;
    float hit_flag; 
    vec4 color;
    vec3 normal;
    float padding;
};

struct RaySample {
    vec2 uv;
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float padding; 
} ubo;

layout(set = 0, binding = 1, scalar) buffer readonly HitDataBuffer {
   HitData hits[];
} hit_buffer;

layout(set = 0, binding = 2, scalar) buffer readonly SampleBuffer {
   RaySample samples[];
} sample_buffer;

layout(push_constant) uniform PushConstants {
    mat4 model;
    int mode;
    float major_radius; 
    float minor_radius;
    float height;
} pc;

layout(location = 0) out vec4 out_color_and_flag;

const float PI = 3.14159265359;

void main() {
    uint index = gl_VertexIndex;
    HitData hit = hit_buffer.hits[index];

    out_color_and_flag = vec4(hit.color.rgb, hit.hit_flag);

    if (hit.hit_flag > 0.0) {
        vec3 final_pos;
        
        if (pc.mode == 1) {
            // MODE 1: REPROJECTION
            vec2 uv = sample_buffer.samples[index].uv;
            
            // REMOVED V-FLIP to match RayGen and C++
            float u = uv.x * 2.0 * PI;
            float v = uv.y * 2.0 * PI; 
            
            float R = pc.major_radius;
            float r = pc.minor_radius;
            float h = pc.height;

            vec3 local_pos;
            local_pos.x = (R + r * cos(v)) * cos(u);
            local_pos.y = r * sin(v) + h; 
            local_pos.z = (R + r * cos(v)) * sin(u);

            // Calculate Normal for offset
            vec3 local_norm;
            local_norm.x = cos(v) * cos(u);
            local_norm.y = sin(v);
            local_norm.z = cos(v) * sin(u);

            // Offset to prevent Z-fighting
            local_pos += local_norm * 0.01; 

            final_pos = (pc.model * vec4(local_pos, 1.0)).xyz;
        } else {
            // MODE 0: ORIGINAL HIT POSITION
            final_pos = hit.hit_pos;
        }

        gl_Position = ubo.proj * ubo.view * vec4(final_pos, 1.0);
        gl_PointSize = 2.0;
    } else {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0); 
    }
}