// This will be the main class for the engine, and where most of the code will go

#include "vk_engine.h"

// SDL holds the main SDL library data for opening a window and input
// SDL_vulkan holds the Vulkan-specific flags and functionality for opening a Vulkan-compatible window and other Vulkan-specific things
#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <chrono>
#include <thread>


constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;



VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init(){
    // We initialize SDL and create a window with it
    SDL_Init(SDL_INIT_VIDEO); // The parameter SDL_INIT_VIDEO is a flag to tell SDL which functionalities we want
                              // In this case, we are selecting video and mouse/keyboard input

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    // Create blank SDL window for our application
    _window = SDL_CreateWindow(
        "Vulkan Engine", // window title
        SDL_WINDOWPOS_UNDEFINED, // window position x (don't care)
        SDL_WINDOWPOS_UNDEFINED, // window position y (don't care)
        _windowExtent.width, // window width in pixels
        _windowExtent.height, // window height in pixels
        window_flags
    );

    // load the core Vulkan structures
    init_vulkan(); // Instance and device creation
    init_swapchain(); // create the swapchain
    init_commands();
    init_sync_structures();


    // evrything went fine
    _isInitialized = true;
}

// SDL is a C library, needs to be explicitly destroyed
void VulkanEngine::cleanup(){
    if(_isInitialized){
        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_device);

        // Basically, destroy object in reverse order of their creation
        for(int i = 0; i < FRAME_OVERLAP; i++){
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr); // It is not possible to destroy individually VkComandBuffer
        }

        vkDestroySwapchainKHR(_device, _swapchain, nullptr); // Images are owned and destroyed by the swapchain

        // destroy swap chain resources
        for(int i = 0; i < _swapchainImageViews.size(); i++){
            vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
        }

        vkDestroyDevice(_device, nullptr);
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messanger);
        vkDestroyInstance(_instance, nullptr);        

        SDL_DestroyWindow(_window);
    }

    loadedEngine = nullptr;
}

void VulkanEngine::draw(){
    vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000); // wait GPU to finish its work
    vkResetFences(_device, 1, &get_current_frame()._renderFence);

    uint32_t swapchainImageIndex;
    vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
}

void VulkanEngine::run(){
    SDL_Event e;
    bool bQuit = false;

    /* uint32_t version;
    vkEnumerateInstanceVersion(&version);
    std::cout << "Vulkan version: " << VK_VERSION_MAJOR(version) << "." 
          << VK_VERSION_MINOR(version) << "." << VK_VERSION_PATCH(version) << std::endl; */

    // main loop
    while(!bQuit){
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0){
            // close the window when the user clicks the X button or alt-f4
            if(e.type == SDL_QUIT){
                bQuit = true;
            }

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

// instance and device creation
void VulkanEngine::init_vulkan(){
    // INSTANCE CREATION

    vkb::InstanceBuilder builder;

    // make the Vulkan instance, with basic debug freatures
    auto inst_ret = builder.set_app_name("Toroidal Rendering")
    .request_validation_layers(bUseValidationLayers)
    .require_api_version(1, _version, 0)
    .use_default_debug_messenger() // catches the log messages that the validation layers will output
    .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // store the instance (so that we can control and in case delete it avoiding leakage)
    _instance = vkb_inst.instance;
    // store the debug messager
    _debug_messanger = vkb_inst.debug_messenger;


    // DEVICE SELECTION

    // get the surface of the window we opened with SDL
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    //vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;

	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

    // We want a GPU that can write to the SDL surface ans supports the vesion set
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
                        .set_minimum_version(1, _version)
                        .set_required_features_13(features)
		                .set_required_features_12(features12)
                        .set_surface(_surface)
                        .select()
                        .value();

    // create the final Vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    // get Graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU,_device,_surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
                    //.use_default_format_selection()
                    .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
                    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync present mode
                    .set_desired_extent(_windowExtent.width, _windowExtent.height)
                    .build()
                    .value();
    
    // Store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    _swapchainImageFormat = vkbSwapchain.image_format;
    
}

void VulkanEngine::init_commands(){
    // create a command pool
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT); // allows for resetting of individual command buffers

    for (int i = 0; i < FRAME_OVERLAP; i++){
        vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool);

        // allocate the default command buffer for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        // We are here allocating one command buffer, so _mainCommandBuffer is a single command buffer
        // If multiple needed, command Buffer must allow them in memory (array of command Buffers or similar structures)
        vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer);
    }
}

void VulkanEngine::init_sync_structures()
{
    //create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info(0);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence);

		vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore);
		vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore);
	}
}