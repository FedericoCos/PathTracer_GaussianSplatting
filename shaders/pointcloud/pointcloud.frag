#version 450

layout(location = 0) in vec4 in_color_and_flag;
layout(location = 0) out vec4 outColor;

void main() {
    if (in_color_and_flag.a < 0.0) { // Check flag from alpha
        discard;
    }
    // Draw all points with their ray color
    outColor = vec4(in_color_and_flag.rgb, 1.0);
}