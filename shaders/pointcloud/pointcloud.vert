#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../rt_datacollect/raytracing.glsl" 

// Binding 0: UBO
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Binding 1: The Ray Tracing Results (Hit positions & Colors)
layout(set = 0, binding = 1, scalar) buffer readonly HitDataBuffer {
   HitData hits[];
} hit_buffer;

// Binding 2: The Original Torus Vertices (To snap back to the surface)
layout(set = 0, binding = 2, scalar) buffer readonly TorusVertexBuffer {
   InputVertex v[];
} torus_buffer;

// Push Constant: Controls which "View" we are currently drawing
layout(push_constant) uniform PushConstants {
    mat4 model; // Torus Model Matrix
    int mode;   // 0 = Point Cloud (World), 1 = Projected (Torus)
} pc;

layout(location = 0) out vec4 out_color_and_flag;

void main() {
    uint index = gl_VertexIndex;
    HitData hit = hit_buffer.hits[index];
    
    // Pass color to fragment shader
    out_color_and_flag = vec4(hit.color.rgb, hit.hit_flag);

    // Check if the ray actually hit something
    if (hit.hit_flag > 0.0) {
        vec3 final_pos;

        if (pc.mode == 1) {
            // MODE 1: PROJECTED (Used in Second Draw Call)
            // Snap the point back to the torus surface
            vec3 local_pos = torus_buffer.v[index].pos;
            final_pos = (pc.model * vec4(local_pos, 1.0)).xyz;
        } else {
            // MODE 0: POINT CLOUD (Used in First Draw Call)
            // Keep the point out in the world where the ray hit
            final_pos = hit.hit_pos;
        }

        gl_Position = ubo.proj * ubo.view * vec4(final_pos, 1.0);
        gl_PointSize = 2.0;
    } else {
        // Ray missed: Clip vertex
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    }
}