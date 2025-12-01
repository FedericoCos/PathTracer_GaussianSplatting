#version 450

layout(location = 0) in vec4 in_color_and_flag;
layout(location = 0) out vec4 outColor;

void main() {
    if (in_color_and_flag.a <= 0.0) {
        discard;
    }

    // Input is LINEAR.
    // If your Swapchain is SRGB (vk::Format::eB8G8R8A8Srgb), the GPU
    // will automatically convert Linear -> SRGB here.
    outColor = vec4(in_color_and_flag.rgb, 1.0);
}