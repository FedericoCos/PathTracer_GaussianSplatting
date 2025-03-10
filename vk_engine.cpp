// This will be the main class for the engine, and where most of the code will go

#include "vk_engine.h"

// SDL holds the main SDL library data for opening a window and input
// SDL_vulkan holds the Vulkan-specific flags and functionality for opening a Vulkan-compatible window and other Vulkan-specific things
#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_vulkan.h"


constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;



VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init(){
    // We initialize SDL and create a window with it
    SDL_Init(SDL_INIT_VIDEO); // The parameter SDL_INIT_VIDEO is a flag to tell SDL which functionalities we want
                              // In this case, we are selecting video and mouse/keyboard input

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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
    // Initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // we'll let use of GPU pointers
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainDeletionQueue.push_function([&](){
        vmaDestroyAllocator(_allocator);
    });

    init_swapchain(); // create the swapchain
    init_commands();
    init_sync_structures();

    init_descriptors();

    init_default_data();
    init_pipelines();

    init_imgui();

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

            // Destroy sync objects
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device ,_frames[i]._swapchainSemaphore, nullptr);

            _frames[i]._deletionQueue.flush();
        }

        // flush the global deletion queue
        _mainDeletionQueue.flush();

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

    get_current_frame()._deletionQueue.flush(); // delete objects created fro the past frame
    vkResetFences(_device, 1, &get_current_frame()._renderFence);

    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    // for window resize
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR){
        resize_requested = true;
        return;
    }

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to beginrecording again
    vkResetCommandBuffer(cmd, 0);

    /**
     * Before recording commands into a command buffer, you must call vkBeginCommandBuffer.
        This resets and prepares the command buffer for recording.
     */
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // tells the drivers that this buffer will only be submitted and executed once

    /* // start of command buffer reccording
    vkBeginCommandBuffer(cmd, &cmdBeginInfo);

    // now, we need to transition the swapchain image into a drawable layout, perform operations, and transition it back
    // for a display optimal layout
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL); // TODO, WK_IMAGE_LAYOUT_GENERAL is not the most optimal for rendering
                                                                                                                              // It is used if you want to write an image from a compute shader
                                                                                                                              // If you want a read-only image or an image to be used with rasterization commands, there are better options

    //make a clear-color from frame number. This will flash with a 120 frame period.
	VkClearColorValue clearValue;
	float flash = std::abs(std::sin(_frameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	//make the swapchain image into presentable mode
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd); */

    /**
     * Difference with previous implementation: now we do not perform the clear on the swapchain
     * Instead we do it on the _drawImage.image. Ince we have cleared the image
     * we transition both the swapchain and the draw image into their layouts for transfer
     * and we execute the copy command 
     */
    _drawExtent.height = std::min(_windowExtent.height, _drawImage.imageExtent.height) * renderScale;
    _drawExtent.width= std::min(_windowExtent.width, _drawImage.imageExtent.width) * renderScale;

    // start of command buffer reccording
    vkBeginCommandBuffer(cmd, &cmdBeginInfo);

    // now, we need to transition the swapchain image into a drawable layout, perform operations, and transition it back
    // for a display optimal layout
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL); // TODO, WK_IMAGE_LAYOUT_GENERAL is not the most optimal for rendering
                                                                                                                              // It is used if you want to write an image from a compute shader
                                                                                                                              // If you want a read-only image or an image to be used with rasterization commands, there are better options	

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    // transition the image to allow rasterized manipulation
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    /**
     * The memory barrier ensures that memory operations (reads and writes) performed by one set of pipeline stages
     * are visible to another set of pipeline stages. Without we could have
     * Missing Synchronization: Your compute shader was drawing the background, and immediately after, your graphics pipeline was trying to render the triangle, but without proper synchronization, these operations could overlap.
        Memory Visibility Issues: The graphics pipeline might be reading from memory locations that the compute shader hadn't finished writing to yet, or worse, both might be writing simultaneously.
        Partial Updates: Some pixels might have been updated by the compute shader while others weren't yet, causing visual artifacts when the triangle was rendered on top.
        Cache Incoherence: Different parts of the GPU might have seen different versions of the image data due to caching effects.
     */
    // Create explicit memory barrier
    VkMemoryBarrier2 memBarrier = {};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memBarrier.pNext = nullptr;
    memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    // Configure the dependency info
    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memBarrier;
    dependencyInfo.bufferMemoryBarrierCount = 0;
    dependencyInfo.pBufferMemoryBarriers = nullptr;
    dependencyInfo.imageMemoryBarrierCount = 0;
    dependencyInfo.pImageMemoryBarriers = nullptr;

    // Execute the barrier
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

	draw_geometry(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	/* // execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _windowExtent);

	// set swapchain image layout to Present so we can show it on the screen
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR); */

    // execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _windowExtent);

	// set swapchain image layout to Attachment Optimal so we can draw it
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //draw imgui into the swapchain image
	draw_imgui(cmd,  _swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can draw it
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	vkEndCommandBuffer(cmd);

    //prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);	
	
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo,&signalInfo,&waitInfo);	

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence);

    //prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	vkQueuePresentKHR(_graphicsQueue, &presentInfo);

	//increase the number of frames drawn
	_frameNumber++;

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

            // send SDL event to imgui for handling
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (resize_requested){
            resize_swapchain();
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale",&renderScale, 0.3f, 1.f);
			
			ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];
		
			ImGui::Text("Selected effect: %s", selected.name);
		
			ImGui::SliderInt("Effect Index", &currentBackgroundEffect,0, backgroundEffects.size() - 1);
		
			ImGui::InputFloat4("data1",(float*)& selected.data.data1);
			ImGui::InputFloat4("data2",(float*)& selected.data.data2);
			ImGui::InputFloat4("data3",(float*)& selected.data.data3);
			ImGui::InputFloat4("data4",(float*)& selected.data.data4);
		}
		ImGui::End();

        // make imgui calcuulate internal draw structures
        ImGui::Render(); // This calculates the vertices/draws/etc that imgui requires to draw the frame

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
                    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT) // If an image has VK_IMAGE_USAGE_TRANSFER_DST_BIT, it can be used in operations that write to it via a transfer command, such as:
                                                                            // Copying data from a buffer to the image (vkCmdCopyBufferToImage)
                                                                            // Copying data from another image (vkCmdCopyImage)
                                                                            // Blitting (scaling/interpolating) images (vkCmdBlitImage)
                                                                            // Clearing the image with vkCmdClearColorImage() or vkCmdClearDepthStencilImage()
                    .build()
                    .value();
    
    // Store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();

    _swapchainImageFormat = vkbSwapchain.image_format;

    // draw image size will match the window
    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    // hardcoding the draw format to 32 bit float
    _drawImage. imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    /**
     * In Vulkan, all images and buffers must fill a UsageFlags with what 
     * they will be used for. This allows the driver to perform optimizations
     * in the background depending on what that buffer or image is going to do later.
     * In our case we want TransferRSC and TransferDST so that we can copy from and into the image,
     * Storage because thats the compute shader can write to it layout, and
     * Color attachment so that we can use graphics pipelines to draw geometry into it
     */
    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    // for the draw image, we want to allocate it from GPU local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); //  flag for GPU only device for fast access

    // allocate and create the image
    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView);

    // add to deletion queues
    _mainDeletionQueue.push_function([this]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
    });
    
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

    // Command pool for imgui
    vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool);

    //allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);
    vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer);
    _mainDeletionQueue.push_function([this](){
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
    });
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

    // fence for imgui
    vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence);
    _mainDeletionQueue.push_function([this]() { vkDestroyFence(_device, _immFence, nullptr);});
}

void VulkanEngine::draw_background(VkCommandBuffer cmd){
    /* // make a clear-color from frame number. This will flash with a 120 fram period
    VkClearColorValue clearValue;
    clearValue = {{ 0.0f, 0.0f, 0.0f, 1.0f}};

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // clear image
    vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange); */

    // bind the gradient drawing compute pipeline
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}


void VulkanEngine::init_descriptors(){
    // create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} // type of image that can be written to from a copute shader
    };

    globalDescriptorAllocator.init_pool(_device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // allocate a descriptor set for our draw image
    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = _drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

    //make sure both the descriptor allocator and the new layout get cleaned up properly
	_mainDeletionQueue.push_function([&]() {
		globalDescriptorAllocator.destroy_pool(_device);

		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
	});
}

void VulkanEngine::init_pipelines(){
    init_background_pipelines();
    init_triangle_pipeline();
    init_mesh_pipeline();
}

void VulkanEngine::init_background_pipelines(){
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout);

    VkShaderModule gradientShader;
    std::filesystem::path path = std::filesystem::current_path();
    std::string stringPath = path.generic_string() + "/shaders/shader.gradient_color.spv";
    if(!vkutil::load_shader_module(stringPath.c_str(), _device, &gradientShader)){
        std::cout << "Error in loading shader \n" << std::endl;
    }

    VkShaderModule skyShader;
    stringPath = path.generic_string() + "/shaders/shader.sky.spv";
    if(!vkutil::load_shader_module(stringPath.c_str(), _device, &skyShader)){
        std::cout << "Error in loading shader \n" << std::endl;
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    // default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &gradient.pipeline);

    // change the shader module only to create the sky shader
    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};
    // default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline);

    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_device, gradientShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);

	_mainDeletionQueue.push_function([=, *this]() {
		vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
		vkDestroyPipeline(_device, sky.pipeline, nullptr);
        vkDestroyPipeline(_device, gradient.pipeline, nullptr);
		});
}


/**
 * The VulkanEngine::immediate_submit function is a utility designed to execute a 
 * small batch of Vulkan commands immediately and synchronously. 
 * This is useful for tasks like uploading resources to the GPU, 
 * transitioning image layouts, or other operations that need to be performed 
 * outside the main render loop.
 */
void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function){
    /**
     * TODO a way to improve this would be to run it on a different queue than the graphics queue, and that way we
     * could overlap the execution from this with the main render loop
     */
    vkResetFences(_device, 1, &_immFence);
    vkResetCommandBuffer(_immCommandBuffer, 0);

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(cmd, &cmdBeginInfo);

    function(cmd);

    vkEndCommandBuffer(cmd);

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    //submit command buffer to the queue and execute it
    // _renderFence will now block until the graphic commands finish execution
    vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence);
    vkWaitForFences(_device, 1, &_immFence, true, 9999999999);
}

void VulkanEngine::init_imgui(){
    /**
     * We'll be drawing imgui directly into the swapchain (unlike compute shaders)
     */

    // 1: create descriptor pool for IMGUI
    // the size of the pool is very oversize, but it's copied from imgui demo
    // itself TODO maybe reduce it? or make it possible to exclude it directly
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool);

    // 2: initialize imgui library

    // this initializes the core structure of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=, *this]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    });
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView){
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::init_triangle_pipeline(){
    VkShaderModule triangleFragShader;
    std::filesystem::path path = std::filesystem::current_path();
    std::string stringPath = path.generic_string() + "/shaders/shader.triangleFrag.spv";
    if(!vkutil::load_shader_module(stringPath.c_str(), _device, &triangleFragShader)){
        std::cout << "Error in loading shader \n" << std::endl;
    }

    VkShaderModule triangleVertexShader;
    stringPath = path.generic_string() + "/shaders/shader.triangleVertex.spv";
    if(!vkutil::load_shader_module(stringPath.c_str(), _device, &triangleVertexShader)){
        std::cout << "Error in loading shader \n" << std::endl;
    }

    // build the pipeline layout that controls the inputs/outputs of the shader
    // we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout);

    PipelineBuilder pipelineBuilder;
    // use the traingle layout we created
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
    // connecting the vertex and pixel shaers to the pipeline
    pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
    // it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // filled trinqagles
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // no multisampling
    pipelineBuilder.set_multisampling_none();
    // no blending
    pipelineBuilder.disable_blending();
    // no depth testing
    pipelineBuilder.disable_depthtest();

    // connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

    // finally build the pipeline
    _trianglePipeline = pipelineBuilder.build_pipeline(_device);

    // clean structures
    vkDestroyShaderModule(_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
		vkDestroyPipeline(_device, _trianglePipeline, nullptr);
    });
}


void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);
	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _drawExtent.width;
	scissor.extent.height = _drawExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	//launch a draw command to draw 3 vertices
	vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

	GPUDrawPushConstants push_constants;
	push_constants.worldMatrix = glm::mat4{ 1.f };
	push_constants.vertexBuffer = rectangle.vertexBufferAddress;

	vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants); // TO CHECK
	vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

	vkCmdEndRendering(cmd);
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage){
    // allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    /**
     * With images, we were creating them in device local memory, which is the fastest
     * memory possible as its on GPU VRAM
     * but with buffers we have to decide if we want them to be writeable from cpu
     * directly or not. These would be the main usages we can use
     * 
        VMA_MEMORY_USAGE_GPU_ONLY Is for purely GPU-local memory. 
        This memory wont be writeable or readable from CPU because its on GPU VRAM, 
        but its the fastest to both read and write with shaders.
        VMA_MEMORY_USAGE_CPU_ONLY Is for memory that is on the CPU RAM. 
        This is memory we can write to from CPU, but the GPU can still read from it. 
        Keep in mind that because this is on CPU ram which is outside of the GPU, 
        the accesses to this will come at a performance hit. 
        It is still quite useful if we have data that changes every frame or 
        small amounts of data where slower access wont matter
        VMA_MEMORY_USAGE_CPU_TO_GPU Is also writeable from CPU, 
        but might be faster to access from GPU. On vulkan 1.2 and forwards, 
        GPUs have a small memory region on their own VRAM that is still writeable from CPU. 
        Its size is limited unless we use Resizable BAR, 
        but its memory that is both cpu-writeable and fast to access in GPU
        VMA_MEMORY_USAGE_GPU_TO_CPU Used on memory that we want to be safely readable 
        from CPU.

     */
    bufferInfo.usage = usage; // We can use this to control where VMA will put our buffer

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // this maps the pointer automatically so we can write to the memory, as long as the buffer is accessible from CPU
    AllocatedBuffer newBuffer;

    // allocate the buffer
    vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info);

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer){
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices){
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    // create vertex buffer
    newSurface.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT /* bacause its a SSBO */ | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT /* Bacause we'll be taking its adress*/,
		VMA_MEMORY_USAGE_GPU_ONLY);

    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);
    

    // create index buffer
    newSurface.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT /* To signal that buffer is for indexed draws */ | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

    /**
     * With the buffers allocated, we need to write the data into them. 
     * For that, we will be using a staging buffer. 
     * This is a very common pattern with vulkan. 
     * As GPU_ONLY memory cant be written on CPU, we first write the memory on a 
     * temporal staging buffer that is CPU writeable, and then execute a 
     * copy command to copy this buffer into the GPU buffers. 
     * Its not necesary for meshes to use GPU_ONLY vertex buffers, 
     * but its highly recommended unless its something like a 
     * CPU side particle system or other dynamic effects.
     */

    AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void * data = staging.allocation -> GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);

    // copy index buffer
    memcpy((char *)data + vertexBufferSize, indices.data(), indexBufferSize);
    

    /**
     * TODO
     * Note that this pattern is not very efficient, 
     * as we are waiting for the GPU command to fully execute before continuing 
     * with our CPU side logic. This is something people generally put on a 
     * background thread, whose sole job is to execute uploads like this one, 
     * and deleting/reusing the staging buffers.
     */
    immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
	});

    destroy_buffer(staging);

    return newSurface;
}

void VulkanEngine::init_mesh_pipeline(){
    VkShaderModule triangleFragShader;
    std::filesystem::path path = std::filesystem::current_path();
    std::string stringPath = path.generic_string() + "/shaders/shader.triangleFrag.spv";
    if(!vkutil::load_shader_module(stringPath.c_str(), _device, &triangleFragShader)){
        std::cout << "Error in loading shader \n" << std::endl;
    }

    VkShaderModule triangleVertexShader;
    stringPath = path.generic_string() + "/shaders/shader.triangleMeshVertex.spv";
    if(!vkutil::load_shader_module(stringPath.c_str(), _device, &triangleVertexShader)){
        std::cout << "Error in loading shader \n" << std::endl;
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;

    vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout);

    PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	pipelineBuilder.disable_blending();
    // pipelineBuilder.enable_blending_alphablend();

	pipelineBuilder.disable_depthtest();

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(VK_FORMAT_UNDEFINED);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
	});
}

void VulkanEngine::init_default_data() {
	std::array<Vertex,4> rect_vertices;

	rect_vertices[0].position = {0.5,-0.5, 0};
	rect_vertices[1].position = {0.5,0.5, 0};
	rect_vertices[2].position = {-0.5,-0.5, 0};
	rect_vertices[3].position = {-0.5,0.5, 0};

	rect_vertices[0].color = {0,0, 0,1};
	rect_vertices[1].color = { 0.5,0.5,0.5 ,1};
	rect_vertices[2].color = { 1,0, 0,1 };
	rect_vertices[3].color = { 0,1, 0,1 };

	std::array<uint32_t,6> rect_indices;

	rect_indices[0] = 0;
	rect_indices[1] = 1;
	rect_indices[2] = 2;

	rect_indices[3] = 2;
	rect_indices[4] = 1;
	rect_indices[5] = 3;

	rectangle = uploadMesh(rect_indices,rect_vertices);

	//delete the rectangle data on engine shutdown
	_mainDeletionQueue.push_function([&](){
		destroy_buffer(rectangle.indexBuffer);
		destroy_buffer(rectangle.vertexBuffer);
	});

}


void VulkanEngine::resize_swapchain(){
    vkQueueWaitIdle(_graphicsQueue);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    resize_requested = false;
}


void VulkanEngine::destroy_swapchain()
{
	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
}