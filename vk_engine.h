// This will be the main class for the engine, and where most of the code will go

#pragma once

#include "vk_types.h"
#include "vk_initializers.h"

#include "VkBootstrap.h"

class VulkanEngine{
public:

    bool _isInitialized{ false };
    int _frameNumber{0};
    int _version{3};

    VkExtent2D _windowExtent{ 1700, 900 };
    struct SDL_Window * _window{ nullptr };

    // Initialization variables
    VkInstance _instance; // Vulkan library handle
    VkDebugUtilsMessengerEXT _debug_messanger; // Vulkan debug output handle
    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device; // Vulkan device for commands
    VkSurfaceKHR _surface; // Vulkan window surface

    // Swapchain variables
    VkSwapchainKHR _swapchain; 
    VkFormat _swapchainImageFormat; // image format expected by the windowing system
    std::vector<VkImage> _swapchainImages; // array of images from the swapchain
    std::vector<VkImageView> _swapchainImageViews; // array of image-views from the swapchain

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    //run main loop
    void run();


private:

    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();

};