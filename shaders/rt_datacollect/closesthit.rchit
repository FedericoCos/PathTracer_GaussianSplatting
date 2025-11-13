#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "raytracing.glsl"

// Payload from the raygen shader
layout(location = 0) rayPayloadInEXT RayPayload payload;

// Output buffer
layout(set = 0, binding = 2, scalar) buffer writeonly OutputHitBuffer {
    HitData hits[];
} output_buffer;


void main()
{
    // Calculate the world-space hit position using the built-in
    vec3 hit_pos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    
    // Write the data to the output buffer at the correct index
    uint index = payload.vertex_index;
    output_buffer.hits[index].hit_pos = hit_pos;
    output_buffer.hits[index].hit_flag = 1.0; // Mark as "hit"
    output_buffer.hits[index].color = vec4(payload.color, 1.0);
}