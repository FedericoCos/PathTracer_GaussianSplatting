#version 450

// --- BINDINGS ---
// UBO containing the light's properties
layout(binding = 0) uniform ShadowUBO {
    mat4 proj;
    mat4 views[6];
    vec4 lightPos;
    float farPlane;
} ubo;

// --- INPUTS ---
layout(location = 0) in vec3 fragLightSpacePos;

void main() {
    // Calculate distance from the light's center to the fragment
    float lightDistance = length(fragLightSpacePos - ubo.lightPos.xyz);
    
    // Normalize distance and write to depth buffer
    // This value is between 0.0 and 1.0
    gl_FragDepth = lightDistance / ubo.farPlane;
}