#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

// GLFW is for window and input management
#include "../Helpers/GLFWhelper.h"

// All the Engine pieces
#include "device.h"



const std::vector validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


class Engine{
public:
    Engine(){

    }

    ~Engine(){

    }


    void init() {
       initWindow();
       initVulkan();
    };

    void run();




private:

    // Window variables
    GLFWwindow *window;
    uint32_t win_width = 1280;
    uint32_t win_height = 800;

    // RAII context
    vk::raii::Context context;

    // Instance variable
    vk::raii::Instance instance = nullptr;

    // Device variables
    Device device;
    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device * logical_device;
    vk::raii::Queue graphics_queue = nullptr;



    // ----- Helper functions
    std::vector<const char*> getRequiredExtensions();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

        return vk::False;
    }

    vk::raii::DebugUtilsMessengerEXT debug_messanger = nullptr;
    void setupDebugMessanger();

    // ----- Init functions
    bool initWindow();
    bool initVulkan();

    void createInstance();


    // ---- Closing functions
    void cleanup();

};


