// main class for the engine

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#endif


#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_vulkan.h"

// --------- PUBLIC FUNCTIONS

void VulkanEngine::init(){
    // Initialize SDL for windown and input
    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags
    );

    // Iitialize Vulkan core structures: instance and device selection
    std::cout << "Initializing Core Structures..." << std::endl;
    init_vulkan();
    std::cout << "Core structures initialized!" << std::endl;


    std::cout << "Initializzing VMA.." << std::endl;
    init_VMA();
    std::cout << "VMA initialized!" << std::endl;

    std::cout << "Initializing swapchain..." << std::endl;
    init_swapchain();
    std::cout << "Swapchain initialized!" << std::endl;

    std::cout << "Initializing images..." << std::endl;
    init_images();
    std::cout << "Images initialized!" << std::endl;

    std::cout << "Initializing commands..." << std::endl;
    init_commands();
    std::cout << "Commands initialized!" << std::endl;

    std::cout << "Initializing sync structures..." << std::endl;
    init_sync_structures();
    std::cout << "Sync Structures initialized!" << std::endl;

    std::cout << "Initializing Descriptors..." << std::endl;
    init_descriptors();
    std::cout << "Descriptors initialized!" << std::endl;

    std::cout << "Initializing Meshes..." << std::endl;
    init_mesh();
    std::cout << "Mesh initialized!" << std::endl;

    std::cout << "Initializing Textures..." << std::endl;
    init_texture();
    std::cout << "Texture initialized!" << std::endl;

    std::cout << "Initializing material..." << std::endl;
    init_materials();
    std::cout << "Material Initialized!" << std::endl;

    std::cout << "Initializing pipelines..." << std::endl;
    init_pipelines();
    std::cout << "Pipelines initialized!" << std::endl;
    
    std::cout << "Initializing Structure scene..." << std::endl;
    init_scene();
    std::cout << "Structure scene Initialized!" << std::endl;


    std::cout << "Initializing imgui..." << std::endl;
    init_imgui();
    std::cout << "Imgui Initialized!" << std::endl;
}

VmaAllocator& VulkanEngine::getAllocator(){
    return _allocator;
}

VkDevice& VulkanEngine::getDevice(){
    return _device;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function){
    /**
     * TODO a way to improve this would be to run it on a different queue than the graphics queue, and that way we
     * could overlap the execution from this with the main render loop
     */
    vkResetFences(_device, 1, &_imgFence);
    vkResetCommandBuffer(_imgCommandBuffer, 0);

    VkCommandBuffer cmd = _imgCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(cmd, &cmdBeginInfo);

    function(cmd);

    vkEndCommandBuffer(cmd);

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    //submit command buffer to the queue and execute it
    // _renderFence will now block until the graphic commands finish execution
    vkQueueSubmit2(_graphicsQueue, 1, &submit, _imgFence);
    vkWaitForFences(_device, 1, &_imgFence, true, 9999999999);
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage){
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // this maps the pointer automatically so we can write to the memory, as long as the buffer is accessible from CPU
    AllocatedBuffer newBuffer;
    vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info);

    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer){
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

bool VulkanEngine::load_shader_module(const char * filePath, VkShaderModule * outShaderModule){
    // open the dile. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if(!file.is_open()){
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg(); // long unsigned int

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char *) buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module,  using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multiply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
        return false;
    }

    *outShaderModule = shaderModule;
    return true;
}

VkDescriptorSetLayout& VulkanEngine::getGpuSceneDataDescriptorLayout(){
    return _gpuSceneDataDescriptorLayout;
}

AllocatedImage& VulkanEngine::getDrawImage(){
    return _drawImage;
}

AllocatedImage& VulkanEngine::getDepthImage(){
    return _depthImage;
}

AllocatedImage& VulkanEngine::getErrorCheckerboardImage(){
    return _errorCheckerboardImage;
}

void VulkanEngine::run(){
    SDL_Event e;
    bool bQuit = false;

    set_data_before_draw();
    
    while(!bQuit){
        auto start = std::chrono::system_clock::now();

        while(SDL_PollEvent(&e) != 0){
            if(e.type == SDL_QUIT){
                bQuit = true;
            }

            if (e.type == SDL_WINDOWEVENT){
                if(e.window.event == SDL_WINDOWEVENT_MINIMIZED){
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED){
                    stop_rendering = false;
                }
            }
            
            _mainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (stop_rendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if(resize_requested){
            resize_swapchain();
        }

        // imgui new frame
        set_imgui();

        draw();

        auto end = std::chrono::system_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        _engineStats.frametime = elapsed.count() / 1000.f;
    }
}

// --------- PRIVATE FUNCTIONS

void VulkanEngine::init_vulkan(){
    // INSTANCE SELECTION
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("Engine")
    .request_validation_layers(_useValidationLayer)
    .require_api_version(1, _version, 0)
    .use_default_debug_messenger()
    .build();

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
    _debug_messanger = vkb_inst.debug_messenger;

    // DEVICE SELECTION
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // v1.3 features
    VkPhysicalDeviceVulkan13Features features13 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    // v1.2 features
    VkPhysicalDeviceVulkan12Features features12 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;


    vkb::PhysicalDeviceSelector selector { vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, _version)
    .set_required_features_13(features13)
    .set_required_features_12(features12)
    .set_surface(_surface)
    .select()
    .value();

    vkb::DeviceBuilder deviceBuilder { physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    _device = vkbDevice.device;
    _physicalDevice = physicalDevice.physical_device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    _presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();
    _presentQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::present).value();
}

void VulkanEngine::init_VMA(){
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainDeletionQueue.push_function([&](){
        vmaDestroyAllocator(_allocator);
    });
}

SwapchainSupportDetails VulkanEngine::query_swapchain_support(){
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr);

    if(presentModeCount != 0){
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

void VulkanEngine::init_swapchain(){
    /* vkb::SwapchainBuilder swapChainBuilder {_physicalDevice, _device, _surface };
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapChainBuilder
    .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
    .set_desired_extent(_windowExtent.width, _windowExtent.height)
    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    .build()
    .value();

    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value(); */

    // -----------------------
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    SwapchainSupportDetails swapChainSupport = query_swapchain_support();

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount){
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }


    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = _swapchainImageFormat;
    createInfo.imageColorSpace =  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = _windowExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t queueFamilyIndices[] = {_graphicsQueueFamily, _presentQueueFamily};
    
    if (_graphicsQueueFamily != _presentQueueFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;


    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE; 

    if (vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, nullptr);
    _swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, _swapchainImages.data());

    _swapchainImageViews.resize(imageCount);
    
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(_device, &viewInfo, nullptr, &_swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
    }
}

void VulkanEngine::init_images(){
    // Initializing Draw Image
    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;
    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView);

    // Initializing Depth image
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
	vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView);

    // add to deletion queues
    /* _mainDeletionQueue.push_function([this]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
	    vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    }); */
}

void VulkanEngine::init_commands(){
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT); // allows for resetting of individual command buffers
    
    for(int i = 0; i < FRAME_OVERLAP; i++){
        vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i].commandPool);
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i].commandPool, 1);
        vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i].mainCommandBuffer); 
    }

    // command buffer for imgui
    vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_imgCommandPool);
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_imgCommandPool, 1);
    vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_imgCommandBuffer);

    _mainDeletionQueue.push_function([this](){
        vkDestroyCommandPool(_device, _imgCommandPool, nullptr);
    });


}

void VulkanEngine::init_sync_structures(){
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT); // starts the fence in signaled stage
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info(0);

    for (int i = 0; i < FRAME_OVERLAP; i++){
        vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i].renderFence);

        vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].swapchainSemaphore);
        vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore);
    }

    vkCreateFence(_device, &fenceCreateInfo, nullptr, &_imgFence);
    _mainDeletionQueue.push_function([this](){
        vkDestroyFence(_device, _imgFence, nullptr);
    });

}

void VulkanEngine::init_descriptors(){
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1} // image that a shader can read from and write to without a sampler
                                              // used for image processing, physic simulations, and to work directly with texel values
    };
    
    _globalDescriptorAllocator.init(_device, 10, sizes);

    // descriptor set layout fro compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    //  make the descriptor set layout for the global uniform buffer
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    // descriptor set layout for texture sampling
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    _drawImageDescriptors = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);
    _mainDeletionQueue.push_function([&](){
        _globalDescriptorAllocator.destroy_pools(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
    });


    DescriptorWriter writer;
    writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _drawImageDescriptors);

    for (int i = 0; i < FRAME_OVERLAP; i++){
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }
        };

        _frames[i].frameDescriptors = new DescriptorAllocatorGrowable{};
        _frames[i].frameDescriptors -> init(_device, 1000, frame_sizes);

        _mainDeletionQueue.push_function([&, i]() {
            _frames[i].frameDescriptors -> destroy_pools(_device);
        });
    }



}

void VulkanEngine::init_mesh(){
    std::filesystem::path path = std::filesystem::current_path();
    std::string stringPath = path.generic_string() + "/assets/structure.glb";
    testMeshes = loadGltfMeshes(*this, stringPath).value();
}

void VulkanEngine::init_texture(){
    uint32_t white =glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage = vkimage::create_image((void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, 
                            VK_IMAGE_USAGE_SAMPLED_BIT, false, *this);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage = vkimage::create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT, false, *this);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage = vkimage::create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT, false, *this);

    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _errorCheckerboardImage = vkimage::create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT, false, *this);

    // We need to create the sampler for the texture
    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

	_mainDeletionQueue.push_function([&](){
		vkDestroySampler(_device,_defaultSamplerNearest,nullptr);
		vkDestroySampler(_device,_defaultSamplerLinear,nullptr);

		vkimage::destroy_image(_whiteImage, *this);
		vkimage::destroy_image(_greyImage, *this);
		vkimage::destroy_image(_blackImage, *this);
		vkimage::destroy_image(_errorCheckerboardImage, *this);
	});
    
}

void VulkanEngine::init_materials(){
    GLTFMetallic_Roughness::MaterialResources materialResources;
    materialResources.colorImage = _errorCheckerboardImage;
    materialResources.colorSampler = _defaultSamplerLinear;
	materialResources.metalRoughImage = _errorCheckerboardImage;
	materialResources.metalRoughSampler = _defaultSamplerLinear;

    AllocatedBuffer materialConstants = create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
    sceneUniformData -> colorFactors = glm::vec4{1, 1, 1, 1};
    sceneUniformData -> metal_rough_factors = glm::vec4{1, 0.5, 0, 0};

    materialResources.dataBuffer = materialConstants.buffer;
    materialResources.dataBufferOffset = 0;

    defaultData = metalRoughMaterial.write_material(_device, MaterialPass::MainColor, materialResources, _globalDescriptorAllocator);

    _mainDeletionQueue.push_function([=, this](){
        destroy_buffer(materialConstants);
    });

    for (auto& m : testMeshes) {
		std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
		newNode->mesh = m;

		newNode->localTransform = glm::mat4{ 1.f };
		newNode->worldTransform = glm::mat4{ 1.f };
		for (auto& s : newNode->mesh->surfaces) {
			s.material = std::make_shared<GLTFMaterial>(defaultData);
		}

		loadedNodes[m->name] = std::move(newNode);
	}
    
}

void VulkanEngine::init_pipelines(){
    init_compute_pipeline();
    metalRoughMaterial.build_pipelines(*this);
}

void VulkanEngine::init_compute_pipeline(){
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
    computeLayout.pushConstantRangeCount = 1;;

    vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_computePipelineLayout);

    VkShaderModule gradientShader;
    std::filesystem::path path = std::filesystem::current_path();
    std::string stringPath = path.generic_string() + "/shaders/shader.gradient.spv";
    if(!load_shader_module(stringPath.c_str(), &gradientShader)){
        std::cout << "Error in loading gradient shader \n" << std::endl;
    }

    VkShaderModule skyShader;
    stringPath = path.generic_string() + "/shaders/shader.sky.spv";
    if(!load_shader_module(stringPath.c_str(), &skyShader)){
        std::cout << "Error in loading sky shader \n" << std::endl;
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = gradientShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _computePipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    ComputeEffect gradient;
    gradient.layout = _computePipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline);

    computePipelineCreateInfo.stage.module = skyShader;
    ComputeEffect sky;
    sky.layout = _computePipelineLayout;
    sky.name = "sky";
    sky.data = {};
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline);

    _computePipelines.push_back(gradient);
    _computePipelines.push_back(sky);

    vkDestroyShaderModule(_device, gradientShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);

    _mainDeletionQueue.push_function([=, *this](){
        vkDestroyPipelineLayout(_device, _computePipelineLayout, nullptr);
        vkDestroyPipeline(_device, sky.pipeline, nullptr);
        vkDestroyPipeline(_device, gradient.pipeline, nullptr);
    });
}

void VulkanEngine::init_imgui(){
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

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _physicalDevice;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

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

void VulkanEngine::init_scene(){
    std::filesystem::path path = std::filesystem::current_path();
    std::string stringPath = path.generic_string() + "/assets/structure.glb";
    auto structureFile = loadGltf(*this, stringPath);

    assert(structureFile.has_value());

    loadedScenes["structure"]  = *structureFile;
}


// ---------------------------------------- DRAW FUNCTION


void VulkanEngine::resize_swapchain(){
    vkQueueWaitIdle(_graphicsQueue);

    destroy_swapchain();
    

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    recreate_swapchain();

    resize_requested = false;

    vkDestroyImageView(_device, _drawImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

    vkDestroyImageView(_device, _depthImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);

    init_images();

    DescriptorWriter writer;
    writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _drawImageDescriptors);


}

void VulkanEngine::destroy_swapchain(){
    for (int i = 0; i < _swapchainImageViews.size(); i++){
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void VulkanEngine::recreate_swapchain(){
    init_swapchain();
}

void VulkanEngine::update_scene(){
    auto start = std::chrono::system_clock::now();

    _mainDrawContext.opaqueSurfaces.clear();
    _mainDrawContext.transparentSurfaces.clear();

    auto temp = std::chrono::system_clock::now();
    auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(temp - _currentTime);
    _currentTime = temp;
    float dt = deltaTime.count() / 1000000.f;

    _mainCamera.update(dt);


    if(_updateStructure)
        loadedScenes["structure"] -> updateNodesRotation(glm::radians(_angle) * dt);
    loadedScenes["structure"]->Draw(glm::mat4{ 1.f }, _mainDrawContext);

    _sceneData.view = _mainCamera.getViewMatrix();;
	_sceneData.proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	_sceneData.proj[1][1] *= -1;
	_sceneData.viewproj = _sceneData.proj * _sceneData.view;

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    _engineStats.scene_update_time = elapsed.count() / 1000.f;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd){
    ComputeEffect& effect = _computePipelines[_currentComputePipeline];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);
    vkCmdPushConstants(cmd, _computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView){
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd){
    _engineStats.drawcall_count = 0;
    _engineStats.triangle_count = 0;
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(_mainDrawContext.opaqueSurfaces.size());

    for (uint32_t i = 0; i < _mainDrawContext.opaqueSurfaces.size(); i++) {
        if (is_visible(_mainDrawContext.opaqueSurfaces[i], _sceneData.viewproj)) {
            opaque_draws.push_back(i);
        }
    }

    // sort the opaque surfaces by material and mesh
    std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
        const RenderObject& A = _mainDrawContext.opaqueSurfaces[iA];
        const RenderObject& B = _mainDrawContext.opaqueSurfaces[iB];
        if (A.material == B.material) {
            return A.indexBuffer < B.indexBuffer;
        }
        else {
            return A.material < B.material;
        }
    });

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    
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

    AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	/* get_current_frame().deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
		}); */

	//write the buffer
	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = _sceneData;

    VkDescriptorSet globalDescriptor = get_current_frame().frameDescriptors -> allocate(_device, _gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);

    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject& r) {
        if (r.material != lastMaterial) {
            lastMaterial = r.material;
            //rebind pipeline and descriptors if the material changed
            if (r.material->pipeline != lastPipeline) {
   
                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,r.material->pipeline->layout, 0, 1,
                    &globalDescriptor, 0, nullptr);
   
               VkViewport viewport = {};
               viewport.x = 0;
               viewport.y = 0;
               viewport.width = (float)_windowExtent.width;
               viewport.height = (float)_windowExtent.height;
               viewport.minDepth = 0.f;
               viewport.maxDepth = 1.f;
   
               vkCmdSetViewport(cmd, 0, 1, &viewport);
   
               VkRect2D scissor = {};
               scissor.offset.x = 0;
               scissor.offset.y = 0;
               scissor.extent.width = _windowExtent.width;
               scissor.extent.height = _windowExtent.height;
   
               vkCmdSetScissor(cmd, 0, 1, &scissor);
            }
   
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
                &r.material->materialSet, 0, nullptr);
        }
       //rebind index buffer if needed
        if (r.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        // calculate final mesh matrix
        GPUDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.vertexBuffer = r.vertexBufferAddress;
   
        vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
   
        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
        _engineStats.drawcall_count++;
        _engineStats.triangle_count += r.indexCount / 3;
    };
    
    for (auto& r : opaque_draws) {
        draw(_mainDrawContext.opaqueSurfaces[r]);
    }
    
    for (auto& r : _mainDrawContext.transparentSurfaces) {
        draw(r);
    }

    vkCmdEndRendering(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    _engineStats.mesh_draw_time = elapsed.count() / 1000.f;
}

void VulkanEngine::draw(){
    update_scene();

    FrameData& current_frame = get_current_frame();
    vkWaitForFences(_device, 1, &current_frame.renderFence, true, 1000000000);
    current_frame.frameDescriptors -> clear_pools(_device);
    vkResetFences(_device, 1, &current_frame.renderFence);

    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, current_frame.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR){
        resize_requested = true;
        return;
    }

    _drawExtent.height = std::min(_windowExtent.height, _drawImage.imageExtent.height);
    _drawExtent.width= std::min(_windowExtent.width, _drawImage.imageExtent.width);

    VkCommandBuffer cmd = current_frame.mainCommandBuffer;
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); // tells the drivers that this buffer will only be submitted and executed once

    vkBeginCommandBuffer(cmd, &cmdBeginInfo);

    // vkimage::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1);
    vkimage::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1, 1);

    draw_background(cmd);

    vkimage::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 1);
    vkimage::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 1, 1);


    /* // Create explicit memory barrier
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
    vkCmdPipelineBarrier2(cmd, &dependencyInfo); */

    draw_geometry(cmd);

    vkimage::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1);
    vkimage::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);

    vkimage::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex],_drawExtent, _windowExtent);
    vkimage::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 1);

    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    vkimage::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 1);

    vkEndCommandBuffer(cmd);

    //prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, current_frame.swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, current_frame.renderSemaphore);	
	
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo,&signalInfo,&waitInfo);	
	vkQueueSubmit2(_graphicsQueue, 1, &submit, current_frame.renderFence);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &current_frame.renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	vkQueuePresentKHR(_presentQueue, &presentInfo);

	//increase the number of frames drawn
	_frameNumber++;
}

void VulkanEngine::set_data_before_draw(){
    _sceneData.sunlightDirection = glm::vec4(0, 1, 0, 1);
    _sceneData.ambientColor = glm::vec4(1, 1, 1, 0.1);
    _sceneData.sunlightColor =  glm::vec4(1, 1, 1, 1);


    _mainCamera.velocity = glm::vec3(0.f);
	_mainCamera.position = glm::vec3(0.f, -00.f, -0.f);;

    _mainCamera.pitch = 0;
    _mainCamera.yaw = 0;

    _currentTime = std::chrono::system_clock::now();
}

void VulkanEngine::set_imgui(){
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("background")) {
        
        ComputeEffect& selected = _computePipelines[_currentComputePipeline];
    
        ImGui::Text("Selected effect: %s", selected.name);
    
        ImGui::SliderInt("Effect Index", &_currentComputePipeline,0, _computePipelines.size() - 1);
    
        ImGui::InputFloat4("data1",(float*)& selected.data.data1);
        ImGui::InputFloat4("data2",(float*)& selected.data.data2);
        ImGui::InputFloat4("data3",(float*)& selected.data.data3);
        ImGui::InputFloat4("data4",(float*)& selected.data.data4);
    }
    ImGui::End();

    ImGui::Begin("Stats");
    float fps = (_engineStats.frametime > 0.0f) ? (1000.0f / _engineStats.frametime) : 0.0f;
    ImGui::Text("frametime %f ms", _engineStats.frametime);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("draw time %f ms", _engineStats.mesh_draw_time);
    ImGui::Text("update time %f ms", _engineStats.scene_update_time);
    ImGui::Text("triangles %i", _engineStats.triangle_count);
    ImGui::Text("draws %i", _engineStats.drawcall_count);
    ImGui::End();

    ImGui::Begin("Camera");
    ImGui::InputFloat("Cam vel", &_mainCamera.speed);
    ImGui::End();

    ImGui::Begin("General Lighting");
    ImGui::InputFloat4("sun dir",(float*)& _sceneData.sunlightDirection);
    ImGui::InputFloat4("sun col",(float*)& _sceneData.sunlightColor);
    ImGui::InputFloat4("amb col",(float*)& _sceneData.ambientColor);
    ImGui::End();

    ImGui::Begin("Structure");
    ImGui::Checkbox("Update structure", &_updateStructure);
    ImGui::InputFloat("rot", &_angle);
    ImGui::End();

    ImGui::Render();
}




// -------------------------------- GLOBAL FUNCTION
bool is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
    std::array<glm::vec3, 8> corners {
        glm::vec3 { 1, 1, 1 },
        glm::vec3 { 1, 1, -1 },
        glm::vec3 { 1, -1, 1 },
        glm::vec3 { 1, -1, -1 },
        glm::vec3 { -1, 1, 1 },
        glm::vec3 { -1, 1, -1 },
        glm::vec3 { -1, -1, 1 },
        glm::vec3 { -1, -1, -1 },
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min = { 1.5, 1.5, 1.5 };
    glm::vec3 max = { -1.5, -1.5, -1.5 };

    for (int c = 0; c < 8; c++) {
        // project each corner into clip space
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3 { v.x, v.y, v.z }, min);
        max = glm::max(glm::vec3 { v.x, v.y, v.z }, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    } else {
        return true;
    }
}
