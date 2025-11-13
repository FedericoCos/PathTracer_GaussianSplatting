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
    // Confirm the "miss"
    uint index = payload.vertex_index;
    output_buffer.hits[index].hit_pos = vec3(0.0);
    output_buffer.hits[index].hit_flag = -1.0;
    output_buffer.hits[index].color = vec4(0.0, 0.0, 0.0, 1.0);
}