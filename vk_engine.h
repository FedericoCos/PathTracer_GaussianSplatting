// This will be the main class for the engine, and where most of the code will go

#pragma once

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"

#include "VkBootstrap.h"

struct FrameData{
    VkCommandPool _commandPool; // the command pool for the commands
    VkCommandBuffer _mainCommandBuffer; // the buffer to record into

    VkSemaphore _swapchainSemaphore, // so that render commands wait on the swapchain image request
                _renderSemaphore; // control presenting the image to the OS once the drawing finishes
    VkFence _renderFence; // waits for the draw commands of a given frame to be finished
};

constexpr unsigned int FRAME_OVERLAP = 2; // double buffering: GPU running some commands while we write into others

class VulkanEngine{
public:

    bool _isInitialized{ false };
    int _frameNumber{0};
    bool stop_rendering{ false };
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
    std::vector<VkImage> _swapchainImages; // array of images from the swapchain to use as texture or to render into
    std::vector<VkImageView> _swapchainImageViews; // array of image-views from the swapchain. Wrappers for images

    // Command variables
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily; // family of the above queue
    FrameData _frames[FRAME_OVERLAP];

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    //run main loop
    void run();

    static VulkanEngine& Get();

    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };


private:

    // Initialization functions
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
};