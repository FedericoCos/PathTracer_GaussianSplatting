#version 450
#extension GL_EXT_multiview : require // <-- MUST have this

// --- BINDINGS ---
layout(binding = 0) uniform ShadowUBO {
    mat4 proj;
    mat4 views[6]; // 6 view matrices
    vec4 lightPos;
    float farPlane;
} ubo;

// --- PUSH CONSTANT ---
layout(push_constant) uniform PushConstants {
    mat4 model;
} pushConstants;

// --- INPUTS ---
layout(location = 0) in vec3 inPosition;

// --- OUTPUTS ---
layout(location = 0) out vec3 fragLightSpacePos;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec4 worldPos = pushConstants.model * vec4(inPosition, 1.0);
    
    // --- THIS IS THE FIX ---
    // Use gl_ViewIndex (0-5) provided by multiview
    gl_Position = ubo.proj * ubo.views[gl_ViewIndex] * worldPos;
    
    fragLightSpacePos = worldPos.xyz;
}