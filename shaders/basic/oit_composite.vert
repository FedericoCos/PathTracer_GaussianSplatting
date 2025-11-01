#version 450

// No inputs
// Outputs clip-space coordinates for a fullscreen triangle
layout(location = 0) out vec2 outTexCoord;

void main() {
    // Hardcoded fullscreen triangle
    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    
    outTexCoord = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}