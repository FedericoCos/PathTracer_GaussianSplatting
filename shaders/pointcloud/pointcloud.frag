#version 450

layout(location = 0) in vec4 in_color_and_flag;
layout(location = 0) out vec4 outColor;

void main() {
    // If hit_flag (stored in alpha) is <= 0.0, discard
    if (in_color_and_flag.a <= 0.0) {
        discard;
    }

    // The color is ALREADY Gamma Corrected in RayGen.
    // Just output it directly.
    outColor = vec4(in_color_and_flag.rgb, 1.0);
}