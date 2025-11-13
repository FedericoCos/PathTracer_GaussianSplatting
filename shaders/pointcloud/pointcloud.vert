#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "../rt_datacollect/raytracing.glsl" // <-- 1. INCLUDE THE SHARED STRUCT

// SET 0, BINDING 0: Main scene UBO
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// SET 0, BINDING 1: Our ray tracing results
// --- 2. USE THE *EXACT* SAME BUFFER DEFINITION ---
layout(set = 0, binding = 1, scalar) buffer readonly HitDataBuffer {
   HitData hits[]; // HitData is from raytracing.glsl
} hit_buffer;
// --- END FIX ---

// Pass the color and hit flag to the fragment shader
layout(location = 0) out vec4 out_color_and_flag;

void main() {
    // --- 3. READ FROM THE CORRECT STRUCT MEMBERS ---
    HitData hit = hit_buffer.hits[gl_VertexIndex];
    vec3 hit_pos = hit.hit_pos;
    vec4 color = hit.color;
    float flag = hit.hit_flag;
    // --- END FIX ---

    // Pass color in .rgb, flag in .a
    out_color_and_flag = vec4(color.rgb, flag);

    if (flag > 0.0) { // Check flag
        // This was a valid hit, draw it
        gl_Position = ubo.proj * ubo.view * vec4(hit_pos, 1.0);
    } else {
        // This was a miss, move the vertex off-screen to clip it
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); 
    }
    
    // Set the size of the point
    gl_PointSize = 2.0;
}