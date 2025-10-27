#version 450

// --- BINDINGS ---
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pushConstants;

// --- INPUTS (Fixed by C++) ---
// These consume locations 0, 1, 2, 3, 4
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec2 inTexCoord;
layout(location = 5) in vec2 inTexCoord1;

// --- OUTPUTS (to fragment shader) ---
// Must start after 4 and not overlap
layout(location = 6) out vec3 fragWorldPos;    
layout(location = 7) out vec3 fragWorldNormal; 
layout(location = 8) out vec2 fragTexCoord;    
layout(location = 9) out mat3 fragTBN;         
layout(location = 12) out vec3 fragColor;  
layout(location = 13) out vec2 fragTexCoord1;
layout(location = 14) out float fragInTangentW;    

void main() {
    vec4 worldPos = pushConstants.model * vec4(inPosition, 1.0);
    
    mat3 normalMatrix = mat3(transpose(inverse(pushConstants.model)));
    
    vec3 T = normalize(vec3(pushConstants.model * vec4(inTangent.xyz, 0.0)));
    vec3 N = normalize(normalMatrix * inNormal);
    vec3 B = cross(N, T) * inTangent.w;
    fragInTangentW = inTangent.w;
    
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = N;
    fragTexCoord = inTexCoord;
    fragTBN = mat3(T, B, N);
    fragColor = inColor;
    fragTexCoord1 = inTexCoord1;
}