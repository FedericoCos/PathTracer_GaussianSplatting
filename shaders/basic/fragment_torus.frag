#version 450

// --- BINDINGS ---
layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metallicRoughnessSampler;

// --- INPUTS (from vertex shader) ---
// These MUST match the 'out' locations from vertex.vert
layout(location = 5) in vec3 fragWorldPos;
layout(location = 6) in vec3 fragWorldNormal;
layout(location = 7) in vec2 fragTexCoord;
layout(location = 8) in mat3 fragTBN;         // Consumes 8, 9, 10
layout(location = 11) in vec3 fragColor;

// --- OUTPUT ---
layout(location = 0) out vec4 outColor;

void main() {
    // --- 1. Sample all material textures ---
    vec4 albedoColor = texture(albedoSampler, fragTexCoord);
    vec3 normalFromMap = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1.0;
    vec2 mr = texture(metallicRoughnessSampler, fragTexCoord).bg; // B=metallic, G=roughness
    float metallic = mr.x;
    float roughness = mr.y;
    
    // --- 2. Calculate Final Normal ---
    // Use this to apply normal maps
    vec3 N = normalize(fragTBN * normalFromMap);
    
    // --- 3. Basic Lighting (Placeholder) ---
    vec3 lightPos = vec3(10.0, 10.0, 10.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 ambient = 0.1 * albedoColor.rgb * fragColor; // Use vertex color

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * albedoColor.rgb * fragColor; // Use vertex color

    vec3 result = ambient + diffuse;
    
    outColor = vec4(result, albedoColor.a);
}