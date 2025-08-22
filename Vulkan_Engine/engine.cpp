#include "engine.h"


#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../Helpers/stb_image.h"


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

uint32_t Engine::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties)
{
    /**
     * Below structure has two arrays
     * - memoryTypes: each type belong to one heap, and specifies how it can be used
     * - memoryHeaps: like GPU VRAM, or system RAM
     */
    vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("failed to find suitable memory type!");
}

static const char* VmaResultToString(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        // add others if needed
        default: return "VkResult(unknown)";
    }
}

void Engine::createBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    AllocatedBuffer &allocated_buffer)
{
    // Prepare VMA alloc info
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

    // Decide flags by host-visible property
    if ((properties & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags{}) {
        // Staging-like buffer: request mapped & sequential write host access
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    } else {
        // Device-only buffer: no special flags (not mapped)
        alloc_info.flags = 0;
    }

    // Fill VkBufferCreateInfo (use raw Vulkan struct via cast)
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;


    VkResult r = vmaCreateBuffer(vma_allocator, reinterpret_cast<VkBufferCreateInfo const*>(&buffer_info), 
            &alloc_info, &allocated_buffer.buffer, &allocated_buffer.allocation, &allocated_buffer.info);
    if (r != VK_SUCCESS) {
        std::stringstream ss;
        ss << "vmaCreateBuffer failed: " << VmaResultToString(r) << " (" << r << ")";
        throw std::runtime_error(ss.str());
    }
}

void Engine::copyBuffer(VkBuffer &src_buffer,VkBuffer &dst_buffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = command_pool_copy;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;
    vk::raii::CommandBuffer command_copy_buffer = std::move(logical_device -> allocateCommandBuffers(alloc_info).front());

    vk::CommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    command_copy_buffer.begin(command_buffer_begin_info);

    command_copy_buffer.copyBuffer(src_buffer, dst_buffer, vk::BufferCopy(0, 0, size));

    command_copy_buffer.end();

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &*command_copy_buffer;
    graphics_presentation_queue.submit(submit_info, nullptr);
    graphics_presentation_queue.waitIdle();
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
    descriptor_set_layout = pipeline_obj.getDescriptorSetLayout();

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.physicalDevice = *physical_device;
    allocator_info.device = **logical_device;
    allocator_info.instance = *instance;              
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_4;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VkResult res = vmaCreateAllocator(&allocator_info, &vma_allocator);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateAllocator failed");
    }

    // Command creation
    createCommandPool();
    createDataBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
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

    pool_info.flags = vk::CommandPoolCreateFlagBits::eTransient;

    command_pool_copy = vk::raii::CommandPool(*logical_device, pool_info);
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

void Engine::createDataBuffer()
{
    vk::DeviceSize vertex_size = sizeof(vertices[0]) * vertices.size();
    vk::DeviceSize index_size = sizeof(indices[0]) * indices.size();
    vk::DeviceSize total_size = vertex_size + index_size;
    index_offset = vertex_size;

    AllocatedBuffer staging_allocated_buffer;

    createBuffer(total_size, vk::BufferUsageFlagBits::eTransferSrc, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
    staging_allocated_buffer);

    void *data;
    vmaMapMemory(vma_allocator, staging_allocated_buffer.allocation, &data);

    memcpy(data, vertices.data(), vertex_size);
    memcpy((char *)data + vertex_size, indices.data(), index_size);

    createBuffer(total_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal, data_buffer);
        
    copyBuffer(staging_allocated_buffer.buffer, data_buffer.buffer, total_size);

    vmaUnmapMemory(vma_allocator, staging_allocated_buffer.allocation);
    vmaDestroyBuffer(vma_allocator,staging_allocated_buffer.buffer, staging_allocated_buffer.allocation);
}

void Engine::createUniformBuffers()
{
    uniform_buffers.clear();
    uniform_buffers_mapped.clear();

    uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
        createBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                uniform_buffers[i]);
        vmaMapMemory(vma_allocator, uniform_buffers[i].allocation, &uniform_buffers_mapped[i]);
    }
}

void Engine::createDescriptorPool()
{
    vk::DescriptorPoolSize pool_size(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT);
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = MAX_FRAMES_IN_FLIGHT;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    descriptor_pool = vk::raii::DescriptorPool(*logical_device, pool_info);
}

void Engine::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, **descriptor_set_layout);
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets.clear();

    descriptor_sets = logical_device -> allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = uniform_buffers[i].buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        vk::WriteDescriptorSet descriptor_write;
        descriptor_write.dstSet = descriptor_sets[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorCount = 1;
        descriptor_write.descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptor_write.pBufferInfo = &buffer_info;

        logical_device -> updateDescriptorSets(descriptor_write, {});
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

    command_buffers[current_frame].bindVertexBuffers(0, {data_buffer.buffer}, {0});
    command_buffers[current_frame].bindIndexBuffer(data_buffer.buffer, index_offset, vk::IndexType::eUint16);

    command_buffers[current_frame].setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(swapchain_extent.width), static_cast<float>(swapchain_extent.height), 0.f, 1.f));
    command_buffers[current_frame].setScissor(0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapchain_extent));
    /**
     * vertexCount -> number of vertices
     * instanceCount -> used for instanced rendering, use 1 if not using it
     * firstVertex -> used as an offset into the vertex buffer, defines the lowest value of SV_VertexId
     * firstInstance -> used as an offset for instanced rendering, defines the lowest value of SV_InstanceID
     */
    //command_buffers[current_frame].draw(vertices.size(), 1, 0, 0);

    command_buffers[current_frame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *graphics_pipeline_layout, 0, *descriptor_sets[current_frame], nullptr);
    command_buffers[current_frame].drawIndexed(indices.size(), 1, 0, 0, 0);


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

    updateUniformBuffer(current_frame);

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

void Engine::updateUniformBuffer(uint32_t current_image)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

    UniformBufferObject ubo{};
    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchain_extent.width) / static_cast<float>(swapchain_extent.height), 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;

    memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
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
    command_pool_copy = nullptr;

    // Destroying uniform buffers objects
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vmaUnmapMemory(vma_allocator, uniform_buffers[i].allocation);
        vmaDestroyBuffer(vma_allocator,uniform_buffers[i].buffer, uniform_buffers[i].allocation);

        descriptor_sets[i] = nullptr;
    }
    descriptor_pool = nullptr;

    // Destroying vertex/index data
    vmaDestroyBuffer(vma_allocator,data_buffer.buffer, data_buffer.allocation);

    // Delete pipeline objs
    pipeline_obj = {};

    // Delete swapchain
    swapchain_obj = {};
    swapchain_image_views.clear();

    // Destroying the allocator
    vmaDestroyAllocator(vma_allocator);

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
