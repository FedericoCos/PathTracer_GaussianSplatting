#include "engine.h"

// ------ Helper Functions
std::vector<const char*> Engine::getRequiredExtensions(){
    uint32_t glfw_extension_count = 0;
    auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
    if(enableValidationLayers){
        extensions.push_back(vk::EXTDebugUtilsExtensionName); // debug messanger extension
    }

    return extensions;
}

void Engine::setupDebugMessanger(){
    if(!enableValidationLayers){
        return;
    }

    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
    vk::DebugUtilsMessageTypeFlagsEXT    message_type_flags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation );
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        {},
        severity_flags,
        message_type_flags,
        &debugCallback,
        nullptr
        };
    debug_messanger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);

}

void Engine::transition_image_layout(
        uint32_t image_index,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask,
        vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask
    ){
        vk::ImageMemoryBarrier2 barrier;
        barrier.srcStageMask = src_stage_mask;
        barrier.srcAccessMask = src_access_mask;
        barrier.dstStageMask = dst_stage_mask;
        barrier.dstAccessMask = dst_access_mask;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapchain_images[image_index];
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vk::DependencyInfo dependency_info;
        dependency_info.dependencyFlags = {};
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &barrier;

        command_buffers[current_frame].pipelineBarrier2(dependency_info);
    }


void Engine::framebufferResizeCallback(GLFWwindow * window, int width, int height){
    auto engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    engine -> framebuffer_resized = true;
}

// ------ Init Functions

bool Engine::initWindow(){
    window = initWindowGLFW("Engine", win_width, win_height);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    if(!window){
        return false;
    }
    return true;
}

bool Engine::initVulkan(){
    createInstance();
    setupDebugMessanger();
    createSurface();

    // Get device and queues
    device_obj.init(instance, surface);
    physical_device = device_obj.getPhysicalDevice();
    logical_device = device_obj.getLogicalDevice(); // It is a pointer, because a copy/move would cause a destruction of the logical device
                                                // IMPORTANT !!!! If physical_device and logical_deice do not need to be accessed here, 
                                                // remove and leave them in device
    graphics_presentation_queue = device_obj.getGraphicsQueue();


    // Get swapchain and swapchain images
    swapchain_obj.init(physical_device, logical_device, surface, window);
    swapchain = swapchain_obj.getSwapchain();
    swapchain_images = swapchain_obj.getSwapchainImages();
    swapchain_image_format = swapchain_obj.getSwapchainImageFormat();
    swapchain_extent = swapchain_obj.getSwapchainExtent();
    for (auto& iv : swapchain_obj.getSwapchainImageViews()) {
        swapchain_image_views.push_back(std::move(iv));
    }

    // Pipeline stage
    pipeline_obj.init(logical_device, swapchain_image_format);
    graphics_pipeline_layout = pipeline_obj.getGraphicsPipelineLayout();
    graphics_pipeline = pipeline_obj.getGraphicsPipeline();


    // Command creation
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();


    return true;
}

void Engine::createInstance(){
    constexpr vk::ApplicationInfo app_info{ 
        "Engine", // application name
        VK_MAKE_VERSION( 1, 0, 0 ), // application version
        "No Engine", // engine name
        VK_MAKE_VERSION( 1, 0, 0 ), // engine version
        vk::ApiVersion14 // API version
    };

    // Get the required validation layers
    std::vector<char const*> required_layers;
    if(enableValidationLayers){
        required_layers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation
    auto layer_properties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(required_layers, [&layer_properties](auto const& required_layer) {
        return std::ranges::none_of(layer_properties,
                                   [required_layer](auto const& layerProperty)
                                   { return strcmp(layerProperty.layerName, required_layer) == 0; });
    }))
    {
        throw std::runtime_error("One or more required layers are not supported!");
    }

    // Get the required instance extensions from GLFW
    auto extensions = getRequiredExtensions();
    auto extensions_properties = context.enumerateInstanceExtensionProperties();
    for(auto const & required_ext : extensions){
        if(std::ranges::none_of(extensions_properties,
        [required_ext](auto const& extension_property){
            return strcmp(extension_property.extensionName, required_ext) == 0;
        })){
            throw std::runtime_error("Required extension not supported: " + std::string(required_ext));
        }
    }

    vk::InstanceCreateInfo createInfo(
        {},                              // flags
        &app_info,                        // application info
        static_cast<uint32_t>(required_layers.size()), // Size of validation layers
        required_layers.data(), // the actual validation layers
        static_cast<uint32_t>(extensions.size()),              // enabled extension count
        extensions.data()                   // enabled extension names
    );

    instance = vk::raii::Instance(context, createInfo);
}

void Engine::createSurface(){
    VkSurfaceKHR _surface;
    if(glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0){
        throw std::runtime_error("Failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
}

void Engine::createCommandPool(){
    vk::CommandPoolCreateInfo pool_info;
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = device_obj.getGraphicsIndex();

    command_pool = vk::raii::CommandPool(*logical_device, pool_info);
}

void Engine::createCommandBuffer(){
    command_buffers.clear();

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = command_pool;
    /**
     * Level parameter specifies if the allocated command buffers are primary or secondary
     * primary -> can be submitted to a queue for execution, but cannot bel called from other command buffers
     * secondary -> cannot be submitted directly, but can be called from primary command buffers
     */
    alloc_info.level = vk::CommandBufferLevel::ePrimary; 
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    command_buffers = vk::raii::CommandBuffers(*logical_device, alloc_info);
}

void Engine::createSyncObjects(){
    present_complete_semaphores.clear();
    render_finished_semaphores.clear();
    in_flight_fences.clear();

    for(size_t i = 0; i < swapchain_images.size(); i++){
        present_complete_semaphores.emplace_back(vk::raii::Semaphore(*logical_device, vk::SemaphoreCreateInfo()));
        render_finished_semaphores.emplace_back(vk::raii::Semaphore(*logical_device, vk::SemaphoreCreateInfo()));
    }

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        in_flight_fences.emplace_back(vk::raii::Fence(*logical_device, {vk::FenceCreateFlagBits::eSignaled}));
    }
}




// ------ Render Loop Functions

void Engine::run(){
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        drawFrame();
    }

    logical_device -> waitIdle();

    cleanup();
}

void Engine::recordCommandBuffer(uint32_t image_index){
    command_buffers[current_frame].begin({});

    transition_image_layout(
        image_index,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );

    vk::ClearValue clear_color = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f);
    vk::RenderingAttachmentInfo attachment_info;
    attachment_info.imageView = swapchain_image_views[image_index];
    attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachment_info.loadOp = vk::AttachmentLoadOp::eClear; // What to do at the image upon loading
    attachment_info.storeOp = vk::AttachmentStoreOp::eStore; // What to do at the image after operations
    attachment_info.clearValue = clear_color;

    vk::RenderingInfo rendering_info;
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = swapchain_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &attachment_info;

    command_buffers[current_frame].beginRendering(rendering_info);

    command_buffers[current_frame].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphics_pipeline);

    command_buffers[current_frame].setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(swapchain_extent.width), static_cast<float>(swapchain_extent.height), 0.f, 1.f));
    command_buffers[current_frame].setScissor(0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapchain_extent));
    /**
     * vertexCount -> number of vertices
     * instanceCount -> used for instanced rendering, use 1 if not using it
     * firstVertex -> used as an offset into the vertex buffer, defines the lowest value of SV_VertexId
     * firstInstance -> used as an offset for instanced rendering, defines the lowest value of SV_InstanceID
     */
    command_buffers[current_frame].draw(3, 1, 0, 0);


    command_buffers[current_frame].endRendering();

    transition_image_layout(
        image_index,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe // a "sink" stage that ensures all prior work is finished before transitioning
    );

    command_buffers[current_frame].end();
}

void Engine::drawFrame(){
    while( vk::Result::eTimeout == logical_device -> waitForFences(*in_flight_fences[current_frame], vk::True, UINT64_MAX));
    
    if (framebuffer_resized) {
        framebuffer_resized = false;
        recreateSwapChain();
    }

    // Waiting for the previous frame to complete
    auto [result, image_index] = swapchain->acquireNextImage(UINT64_MAX, *present_complete_semaphores[semaphore_index], nullptr);

    if(result == vk::Result::eErrorOutOfDateKHR){
        framebuffer_resized = false;
        recreateSwapChain();
        return;
    }
    if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR){
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    logical_device -> resetFences(*in_flight_fences[current_frame]);
    command_buffers[current_frame].reset();
    recordCommandBuffer(image_index);

    vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit_info;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &*present_complete_semaphores[semaphore_index];
    submit_info.pWaitDstStageMask = &wait_destination_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &*command_buffers[current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &*render_finished_semaphores[image_index];

    graphics_presentation_queue.submit(submit_info, *in_flight_fences[current_frame]);

    vk::PresentInfoKHR present_info_KHR;
    present_info_KHR.waitSemaphoreCount = 1;
    present_info_KHR.pWaitSemaphores = &*render_finished_semaphores[image_index];
    present_info_KHR.swapchainCount = 1;
    present_info_KHR.pSwapchains = &**swapchain;
    present_info_KHR.pImageIndices = &image_index;

    result = graphics_presentation_queue.presentKHR(present_info_KHR);
    switch(result){
        case vk::Result::eSuccess: break;
        case vk::Result::eSuboptimalKHR:
            std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
            break;
        default: break; // an unexpected result is returned
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    semaphore_index = (semaphore_index + 1) % present_complete_semaphores.size();


}

void Engine::recreateSwapChain(){
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0){
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    logical_device -> waitIdle();

    swapchain_image_views.clear();
    swapchain_obj.recreateSwapChain(physical_device, logical_device, surface, window);
    

    swapchain = swapchain_obj.getSwapchain();
    swapchain_images = swapchain_obj.getSwapchainImages();
    swapchain_image_format = swapchain_obj.getSwapchainImageFormat();
    swapchain_extent = swapchain_obj.getSwapchainExtent();
    for (auto& iv : swapchain_obj.getSwapchainImageViews()) {
        swapchain_image_views.push_back(std::move(iv));
    }

}




// ------ Closing functions

void Engine::cleanup(){
    // Destroying sync objects
    in_flight_fences.clear();
    render_finished_semaphores.clear();
    present_complete_semaphores.clear();

    // Destroying sync objects
    command_buffers.clear();
    command_pool = nullptr;

    // Delete pipeline objs
    pipeline_obj = {};

    // Delete swapchain
    swapchain_obj = {};
    swapchain_image_views.clear();

    // delete device
    device_obj = {};

    // Delete instance and windowig
    surface = nullptr;
    debug_messanger = nullptr;
    instance = nullptr;
    context = {};


    // Destroy window
    glfwDestroyWindow(window);
    glfwTerminate();
}
