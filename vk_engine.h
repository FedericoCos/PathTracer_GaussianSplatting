// This will be the main class for the engine, and where most of the code will go

#pragma once

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

#include "VkBootstrap.h"

#include <deque>
#include <bits/stdc++.h>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // allows the use of not packed (minimal) structures, more
                                           // in line with Vulkan need
#include <glm/glm.hpp>

constexpr unsigned int FRAME_OVERLAP = 2; // double buffering: GPU running some commands while we write into others

// Push constant for gradient_color shader
struct ComputePushConstants{
    glm::vec4 data1;
    glm::vec4 data2;

};

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

    DeletionQueue _mainDeletionQueue;

    // Virtual memory allocator
    VmaAllocator _allocator;

    // draw resources
    AllocatedImage _drawImage;
    VkExtent2D _drawExtent;

    // DescriptorsSets variables
    DescriptorAllocator globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // gradient (compute) pipeline
    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;

    // immediate submit structures (for ImGui)
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

    //run main loop
    void run();

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    static VulkanEngine& Get();

    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };


private:

    // Initialization functions
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipelines();
    void init_background_pipelines();
    void init_imgui();
    void draw_background(VkCommandBuffer cmd);
};