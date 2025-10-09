#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Define a clear blue color with 50% transparency
    float transparency = 0.5;
    vec3 clearBlue = vec3(0.0, 0.5, 1.0);

    // Output the final color with the desired alpha value
    outColor = vec4(clearBlue, transparency);
}