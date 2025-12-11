// Helper class for window and input management

#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

GLFWwindow * initWindowGLFW(const char* window_title, int& out_width, int& out_height);

void handle_input(GLFWwindow *window, int key, int, int action, int);
