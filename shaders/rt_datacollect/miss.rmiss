#version 460
#extension GL_EXT_ray_tracing : require

#define RAY_TRACING
#include "raytracing.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.hit_flag = -1.0;
    payload.hit_pos = vec3(0.0);
    payload.normal = vec3(0.0, 1.0, 0.0);
    // Background color (e.g., dark gray or sky color)
    payload.color = vec3(0.05, 0.05, 0.08); 
}