#include "GLFWhelper.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

GLFWwindow * initWindowGLFW(const char *windowTitle, uint32_t& out_width, uint32_t& out_height){
    glfwSetErrorCallback([](int error, const char* description){
        printf("GLFW Error (%i): %s\n", error, description);
    }); // error callback via a lambda to catch potential errors

    if(!glfwInit()){
        return nullptr;
    }

    // get windows dimension (fullscreen if one dimension is collapsed)
    const bool wants_whole_area = !out_width || !out_height;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, wants_whole_area ? GLFW_FALSE : GLFW_TRUE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    int x = 0;
    int y = 0;
    int w = mode -> width;
    int h = mode -> height;

    if(wants_whole_area){
        glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
    }
    else{
        w = out_width;
        h = out_height;
    }

    // create a window and retrieve the actual dimensions
    GLFWwindow *window = glfwCreateWindow(w, h, windowTitle, nullptr, nullptr);

    if(!window){
        glfwTerminate();
        return nullptr;
    }

    if(wants_whole_area){
        glfwSetWindowPos(window, x, y);
    }
    out_width = (uint32_t)w;
    out_height = (uint32_t)h;

    return window;
}