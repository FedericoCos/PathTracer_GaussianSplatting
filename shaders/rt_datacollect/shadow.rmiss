#version 460
#extension GL_EXT_ray_tracing : require

#define RAY_TRACING
#include "raytracing.glsl"

layout(location = 1) rayPayloadInEXT ShadowPayload shadowPayload;

void main()
{
    shadowPayload.isHit = false; // Ray missed geometry -> Light is visible
}