#version 450

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

// Samplers for our OIT buffers
// We read from the *resolved* (non-MSAA) images
layout(binding = 0) uniform sampler2D accumSampler;
layout(binding = 1) uniform sampler2D revealSampler;

void main() {
    // Read from the OIT buffers
    vec4 accum = texture(accumSampler, inTexCoord);
    float reveal = texture(revealSampler, inTexCoord).r;

    // Avoid divide-by-zero if alpha (and thus accum.a) is zero
    vec3 avg_color = accum.rgb / max(accum.a, 0.00001);
    
    // The final alpha is (1.0 - revealage)
    float alpha = 1.0 - reveal;

    // Output the transparent color and alpha.
    // The pipeline will use standard alpha blending
    // to blend this *over* the opaque scene.
    outColor = vec4(avg_color, alpha);
}