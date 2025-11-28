#version 450

layout(location = 0) in vec4 in_color_and_flag;
layout(location = 0) out vec4 outColor;

void main() {
    // If hit_flag (stored in alpha) is -1.0, discard
    if (in_color_and_flag.a < 0.0) {
        discard;
    }
    // Output the color calculated by RayGen
    outColor = vec4(in_color_and_flag.rgb, 1.0);
}