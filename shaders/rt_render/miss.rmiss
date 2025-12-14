#version 460
#extension GL_EXT_ray_tracing : require

#define RAY_TRACING
#include "raytracing.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.hit_flag = -1.0;
    vec3 sky = ubo.ambientLight.xyz;
    payload.color = sky * 2.0;
    payload.weight = vec3(1.0);
}