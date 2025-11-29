#version 460
#extension GL_EXT_ray_tracing : require

#define RAY_TRACING
#include "raytracing.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.hit_flag = -1.0; 
    
    // Simple gradient sky
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    // vec3 sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t); // White to Blue
    vec3 sky = vec3(1.0);
    
    // Boost intensity so glass looks bright
    payload.color = sky * 2.0; 
    
    payload.weight = vec3(1.0);
    payload.hit_pos = vec3(0.0);
    payload.normal = vec3(0.0, 1.0, 0.0);
}