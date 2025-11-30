#version 460
#extension GL_EXT_scalar_block_layout : require

#include "../rt_datacollect/raytracing.glsl" 

// Binding 0: Uniform Buffer (View/Proj) - Now correctly uses Binding 0
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float padding; 
} ubo;

// Binding 1: The Hit Data (Written by RayGen)
layout(set = 0, binding = 1, scalar) buffer readonly HitDataBuffer {
   HitData hits[];
} hit_buffer;

// Binding 2: The Ray Samples (UVs on Torus)
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

    // Only draw if we actually hit something (flag > 0)
    if (hit.hit_flag > 0.0) {
        vec3 final_pos;
        
        if (pc.mode == 1) {
            // MODE 1: REPROJECTION ON TORUS SURFACE
            vec2 uv = sample_buffer.samples[index].uv;
            
            // Flip V to match RayGen logic
            float u = uv.x * 2.0 * PI;
            float v = (1.0 - uv.y) * 2.0 * PI; 
            
            float R = pc.major_radius;
            float r = pc.minor_radius;
            float h = pc.height;

            vec3 local_pos;
            local_pos.x = (R + r * cos(v)) * cos(u);
            local_pos.y = r * sin(v) + h; 
            local_pos.z = (R + r * cos(v)) * sin(u);

            final_pos = (pc.model * vec4(local_pos, 1.0)).xyz;
        } else {
            // MODE 0: ORIGINAL POINT CLOUD
            final_pos = hit.hit_pos;
        }

        gl_Position = ubo.proj * ubo.view * vec4(final_pos, 1.0);
        gl_PointSize = 2.0;
    } else {
        // Move invalid points behind camera to cull them
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0); 
    }
}