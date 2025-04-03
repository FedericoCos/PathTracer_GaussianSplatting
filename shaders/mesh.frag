#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	float lightValue = max(dot(inNormal, normalize(sceneData.sunlightDirection.xyz)), 0.1f);

	vec4 texColor = texture(colorTex, inUV);
	vec3 color = inColor * texColor.xyz;
	vec3 ambient = normalize(sceneData.ambientColor.xyz) * sceneData.ambientColor.w;

	outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient , texColor.a / 4);
}