#include "engine.h"


#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "../Helpers/stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../Helpers/tiny_obj_loader.h"


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

Gameobject Engine::createDebugCube()
{
    Gameobject cube;
    cube.vertices.resize(8);
    cube.vertices[0].pos = {-0.5f, -0.5f, -0.5f};
    cube.vertices[1].pos = { 0.5f, -0.5f, -0.5f};
    cube.vertices[2].pos = { 0.5f,  0.5f, -0.5f};
    cube.vertices[3].pos = {-0.5f,  0.5f, -0.5f};
    cube.vertices[4].pos = {-0.5f, -0.5f,  0.5f};
    cube.vertices[5].pos = { 0.5f, -0.5f,  0.5f};
    cube.vertices[6].pos = { 0.5f,  0.5f,  0.5f};
    cube.vertices[7].pos = {-0.5f,  0.5f,  0.5f};

    // Set default values for other attributes to satisfy the PBR pipeline
    for(auto& v : cube.vertices) {
        v.normal = {0.0f, 1.0f, 0.0f};
        v.color = {1.0f, 1.0f, 1.0f};
        v.tex_coord = {0.0f, 0.0f};
        v.tex_coord_1 = {0.0f, 0.0f};
        v.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
    }

    cube.indices = {
        // front
        0, 1, 2, 2, 3, 0,
        // back
        4, 5, 6, 6, 7, 4,
        // left
        4, 7, 3, 3, 0, 4,
        // right
        1, 5, 6, 6, 2, 1,
        // top
        3, 2, 6, 6, 7, 3,
        // bottom
        0, 1, 5, 5, 4, 0
    };

    // Create the GPU buffer for the cube
    createModel(cube);
    return cube;
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
        barrier.image = swapchain.images[image_index];
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vk::DependencyInfo dependency_info;
        dependency_info.dependencyFlags = {};
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &barrier;

        graphics_command_buffer[current_frame].pipelineBarrier2(dependency_info);
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

vk::SampleCountFlagBits Engine::getMaxUsableSampleCount()
{
    vk::PhysicalDeviceProperties physicalDeviceProperties = physical_device.getProperties();

    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

    return vk::SampleCountFlagBits::e1;
}

void Engine::createModel(Gameobject &obj)
{
    // Create and fill the GPU
    vk::DeviceSize vertex_size = sizeof(Vertex) * obj.vertices.size();
    vk::DeviceSize index_size = sizeof(uint32_t) * obj.indices.size();
    vk::DeviceSize total_size = vertex_size + index_size;
    
    // Store the offset where the index buffer begins
    obj.index_buffer_offset = vertex_size;
    
    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, total_size, vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);

    void * data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, obj.vertices.data(), (size_t)vertex_size);
    memcpy((char *)data + vertex_size, obj.indices.data(), (size_t)index_size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    createBuffer(vma_allocator, total_size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer |vk::BufferUsageFlagBits::eIndexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal, obj.geometry_buffer);
    
    copyBuffer(staging_buffer.buffer, obj.geometry_buffer.buffer, total_size,
                command_pool_transfer, &logical_device, transfer_queue);
}

void Engine::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (!engine) return;

    auto it = engine -> key_mapping.find(key);
    if (it == engine -> key_mapping.end())
        return; // key not mapped

    bool pressed = (action == GLFW_REPEAT || action == GLFW_PRESS);
    if(pressed){
        engine -> pressed_keys.insert(key);
    }
    else{
        engine -> pressed_keys.erase(key);
    }
    auto& input = engine->input;

    input.move = glm::vec2(0.f);

    static bool is_left = false;
    static bool is_down = false;

    // Horizontal input
    if(engine -> pressed_keys.count(GLFW_KEY_A) && (key == GLFW_KEY_A || is_left)){
        input.move.x = -1.f;
        is_left = true;
    } else if(!pressed && key == GLFW_KEY_A){
        is_left = false;
    }
    if(engine -> pressed_keys.count(GLFW_KEY_D) && (key == GLFW_KEY_D || !is_left)){
        input.move.x = 1.f;
        is_left = false;
    } else if(!pressed && key == GLFW_KEY_D){
        is_left = true;
        input.move.x = engine -> pressed_keys.count(GLFW_KEY_A) ? -1.f : 0.f;
    }

    // Vertical input
    if(engine -> pressed_keys.count(GLFW_KEY_W) && (key == GLFW_KEY_W || !is_down)){
        input.move.y = 1.f;
        is_down = false;
    } else if(!pressed && key == GLFW_KEY_W){
        is_down = true;
    }
    if(engine -> pressed_keys.count(GLFW_KEY_S) && (key == GLFW_KEY_S || is_down)){
        input.move.y = -1.f;
        is_down = true;
    } else if(!pressed && key == GLFW_KEY_S){
        is_down = false;
        input.move.y = engine -> pressed_keys.count(GLFW_KEY_W) ? 1.f : 0.f;
    }

    Action act = it->second;
    switch (act) {
        case Action::SPEED_UP:      input.speed_up   = pressed; break;
        case Action::SPEED_DOWN:    input.speed_down = pressed; break;
        case Action::ROT_DOWN:   input.rot_up   = pressed; break;
        case Action::ROT_UP:  input.rot_down  = pressed; break;

        case Action::FOV_UP:  input.fov_up     = pressed; break;
        case Action::FOV_DOWN:  input.fov_down   = pressed; break;
        case Action::HEIGHT_UP: input.height_up = pressed; break;
        case Action::HEIGHT_DOWN: input.height_down = pressed; break;
        case Action::RESET: input.reset = pressed; break;
        case Action::SWITCH: input.change = pressed; break;

        case Action::MAJ_RAD_UP: input.maj_rad_up = pressed; break;
        case Action::MAJ_RAD_DOWN: input.maj_rad_down = pressed; break;
        case Action::MIN_RAD_UP: input.min_rad_up = pressed; break;
        case Action::MIN_RAD_DOWN: input.min_rad_down = pressed; break;
    }

    input.consumed  = (input.speed_up || input.speed_down || 
                        input.rot_up || input.rot_down ||
                        input.fov_up || input.fov_down) && input.consumed;
}


void Engine::mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    if(!engine){
        return;
    }

    if(button == GLFW_MOUSE_BUTTON_LEFT){
        engine -> input.left_mouse = (action == GLFW_PRESS);
    }
}

void Engine::cursor_position_callback(GLFWwindow *window, double x_pos, double y_pos)
{
    Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    if(!engine){
        return;
    }

    static bool first_mouse = true;
    static double last_x, last_y;

    if(first_mouse){
        last_x = x_pos;
        last_y = y_pos;
        first_mouse = false;
    }

    double x_offset = x_pos - last_x;
    double y_offset = y_pos - last_y;

    last_x = x_pos;
    last_y = y_pos;

    if(engine -> input.left_mouse){
        engine -> input.look_x = static_cast<float>(-x_offset);
        engine -> input.look_y = static_cast<float>(-y_offset);
    }
    else{
        engine -> input.look_x = 0.f;
        engine -> input.look_y = 0.f;
        first_mouse = true;
    }
}

// ------ Init Functions

bool Engine::initWindow(){
    window = initWindowGLFW("Engine", win_width, win_height);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

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
    physical_device = Device::pickPhysicalDevice(*this);
    mssa_samples = getMaxUsableSampleCount();
    logical_device = Device::createLogicalDevice(*this, queue_indices); 
    graphics_queue = Device::getQueue(*this, queue_indices.graphics_family.value());
    present_queue = Device::getQueue(*this, queue_indices.present_family.value());
    transfer_queue = Device::getQueue(*this, queue_indices.transfer_family.value());

    // Get swapchain and swapchain images
    swapchain = Swapchain::createSwapChain(*this);
    if(swapchain.image_views.empty() || swapchain.image_views.size() <= 0){
        std::cout << "Problem with the image views" << std::endl;
    }
    if(swapchain.images.empty() || swapchain.images.size() <= 0){
        std::cout << "Problem with the images" << std::endl;
    }

    // Memory Allocator stage
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.physicalDevice = *physical_device;
    allocator_info.device = *logical_device;
    allocator_info.instance = *instance;              
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_4;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VkResult res = vmaCreateAllocator(&allocator_info, &vma_allocator);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateAllocator failed");
    }

    // Command creation
    createCommandPool();

    // TO FIX THIS
    color_image = Image::createImage(swapchain.extent.width, swapchain.extent.height,
                        1, mssa_samples, swapchain.format, vk::ImageTiling::eOptimal, 
                    vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, *this);
    color_image.image_view = Image::createImageView(color_image, *this);
    Image::createDepthResources(physical_device, depth_image, swapchain.extent.width, swapchain.extent.height, *this);

    // PIPELINE CREATION
    createPipelines();

    loadObjects("resources/scene.json");
    debug_cube = createDebugCube();
    createTorusModel();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsCommandBuffers();
    createSyncObjects();

    camera = Camera(swapchain.extent.width * 1.0 / swapchain.extent.height);
    
    prev_time = std::chrono::high_resolution_clock::now();

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
    pool_info.queueFamilyIndex = queue_indices.graphics_family.value();

    command_pool_graphics = vk::raii::CommandPool(logical_device, pool_info);

    pool_info.flags = vk::CommandPoolCreateFlagBits::eTransient;
    pool_info.queueFamilyIndex = queue_indices.transfer_family.value();

    command_pool_transfer = vk::raii::CommandPool(logical_device, pool_info);
}

void Engine::createPipelines()
{
    // Creating the pipeline of the objects in the scene
    // Here, we fill the pipeline key -> pipeline obj map to later use it with the objects

    // Pipeline of pbr objects
    PipelineKey p_key;
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        // Binding 0: Uniform Buffer (View/Proj) - Vertex Shader
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, 
                                       vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 1: Albedo/BaseColor Texture - Fragment Shader
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, 
                                       vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 2: Normal Map Texture - Fragment Shader
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1,
                                       vk::ShaderStageFlagBits::eFragment, nullptr),             
        // Binding 3: Metallic/Roughness Texture - Fragment Shader
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1,
                                       vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 4: Ambient Occlusion (AO)
        vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, 
                                       vk::ShaderStageFlagBits::eFragment, nullptr),                 
        // Binding 5: Emissive
        vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eCombinedImageSampler, 1,
                                       vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 6: Clearcoat Texture <-- REMOVED
        // vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eCombinedImageSampler, 1, 
        //                             vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 7: Clearcoat Roughness Texture <-- REMOVED
        // vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eCombinedImageSampler, 1,
        //                             vk::ShaderStageFlagBits::eFragment, nullptr)
    };
    
    // Key definition
    p_key.v_shader = v_shader_pbr;
    p_key.f_shader = f_shader_pbr;
    p_key.is_transparent = false;
    p_key.cull_mode = vk::CullModeFlagBits::eBack;

    // Pipeline creation
    PipelineInfo& p_info = p_p_map[p_key];
    p_info.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    p_info.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &p_info,
        p_key.v_shader,
        p_key.f_shader,
        p_key.is_transparent,
        p_key.cull_mode
    );

    // transparent variation
    p_key.is_transparent = true;
    PipelineInfo& p_info_transparent = p_p_map[p_key];
    p_info_transparent.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    p_info_transparent.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &p_info_transparent,
        p_key.v_shader,
        p_key.f_shader,
        p_key.is_transparent,
        p_key.cull_mode
    );


    // Pipeline for the torus
    bindings = {
        // Binding 0: Uniform Buffer (View/Proj) - Vertex Shader
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, 
                                       vk::ShaderStageFlagBits::eVertex, nullptr)
    };
    p_key.v_shader = v_shader_torus;
    p_key.f_shader = f_shader_torus;
    p_key.is_transparent = true;
    p_key.cull_mode = vk::CullModeFlagBits::eBack;

    PipelineInfo& p_info_torus = p_p_map[p_key];
    p_info_torus.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    p_info_torus.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &p_info_torus,
        p_key.v_shader,
        p_key.f_shader,
        p_key.is_transparent,
        p_key.cull_mode
    );

}

void Engine::loadObjects(const std::string &scene_path)
{
    // Open the scene file
    std::ifstream scene_file(scene_path);
    if (!scene_file.is_open()) {
        throw std::runtime_error("Failed to open scene file: " + scene_path);
    }
    // Parse the JSON data
    json scene_data = json::parse(scene_file);

    // Iterate over each object definition in the JSON
    for (const auto& obj_def : scene_data) {
        P_object new_object;

        std::string model_path = obj_def["model"];
        new_object.loadModel(model_path, *this);
        
        // Create the GPU buffer for the model
        createModel(new_object);

       //  new_object.createMaterialDescriptorSets(*this);

        // Set the object's transform from the JSON data
        if (obj_def.contains("position")) {
            new_object.changePosition({
                obj_def["position"][0],
                obj_def["position"][1],
                obj_def["position"][2]
            });
        }
        if (obj_def.contains("scale")) {
            new_object.changeScale({
                obj_def["scale"][0],
                obj_def["scale"][1],
                obj_def["scale"][2]
            });
        }
        if (obj_def.contains("rotation")) {
            new_object.changeRotation({
                obj_def["rotation"][0],
                obj_def["rotation"][1],
                obj_def["rotation"][2]
            });
        }

        // Pipeline creation
        PipelineKey p_key;
        p_key.v_shader = "shaders/basic/vertex.spv";
        p_key.f_shader = "shaders/basic/fragment.spv";
        p_key.is_transparent = false;
        p_key.cull_mode = vk::CullModeFlagBits::eBack;

        if(!p_p_map.contains(p_key)){
            std::cout << "Error! Pipeline not present\nPipeline info: v_shader->" << p_key.v_shader << " f_shader->" <<
            p_key.f_shader << std::endl;
        }
        new_object.o_pipeline = &p_p_map[p_key];
        p_key.is_transparent = true;
        new_object.t_pipeline = &p_p_map[p_key];

        scene_objs.emplace_back(std::move(new_object));

        // TO REMOVE
        /* std::vector<Gameobject>& arr = p_o_map[p_key];
        arr.emplace_back(std::move(new_object)); */
    }
}

void Engine::createTorusModel()
{
    torus.generateMesh(40.f, 1.f, 0.5f, 200, 80);
    Primitive torusPrim;
    torusPrim.index_count = torus.indices.size();
    torusPrim.first_index = 0;
    torusPrim.material_index = 0;
    torus.o_primitives.push_back(torusPrim);

    Material torusMat;
    torusMat.albedo_texture_index = -1; // No texture, shader should use baseColorFactor
    torusMat.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% transparent white
    torus.materials.push_back(std::move(torusMat));

    createModel(torus); 

    // Setup torus pipeline
    /* PipelineKey p_key;
    p_key.v_shader = v_shader_torus;
    p_key.f_shader = f_shader_torus;
    p_key.is_transparent = true;
    p_key.cull_mode = vk::CullModeFlagBits::eBack;

    if(!p_p_map.contains(p_key)){
        std::cout << "Error! Pipeline not present\nPipeline info: v_shader->" << p_key.v_shader << " f_shader->" <<
        p_key.f_shader << std::endl;
    }
    torus.pipeline = &p_p_map[p_key]; */
}

void Engine::createUniformBuffers()
{
    uniform_buffers.clear();
    uniform_buffers_mapped.clear();

    uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
        createBuffer(vma_allocator, buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                uniform_buffers[i]);
        vmaMapMemory(vma_allocator, uniform_buffers[i].allocation, &uniform_buffers_mapped[i]);
    }
}

void Engine::createDescriptorPool()
{
    uint32_t object_count = 0;
    uint32_t total_materials = 0;

    for(auto& obj : scene_objs){
        total_materials += obj.materials.size();
    }
    // total_materials += torus.materials.size(); // Add torus materials

    // We now allocate per-material, not per-object
    uint32_t total_sets = total_materials * MAX_FRAMES_IN_FLIGHT;
    
    // This MUST match your descriptor set layout
    std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, total_sets),
        // 5 samplers (Albedo, Normal, Met/Rough, AO, Emissive) per set
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, total_sets * 5) // <-- MODIFIED (7 to 5)
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = total_sets;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size()); 
    pool_info.pPoolSizes = pool_sizes.data();

    descriptor_pool = vk::raii::DescriptorPool(logical_device, pool_info);
}

void Engine::createDescriptorSets()
{
    for(auto& obj : scene_objs){
        obj.createMaterialDescriptorSets(*this);
    }

    // torus.createMaterialDescriptorSets(*this);
}

void Engine::createGraphicsCommandBuffers(){
    graphics_command_buffer.clear();

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = command_pool_graphics;
    /**
     * Level parameter specifies if the allocated command buffers are primary or secondary
     * primary -> can be submitted to a queue for execution, but cannot bel called from other command buffers
     * secondary -> cannot be submitted directly, but can be called from primary command buffers
     */
    alloc_info.level = vk::CommandBufferLevel::ePrimary; 
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    graphics_command_buffer = vk::raii::CommandBuffers(logical_device, alloc_info);
}

void Engine::createSyncObjects(){
    present_complete_semaphores.clear();
    render_finished_semaphores.clear();
    in_flight_fences.clear();

    for(size_t i = 0; i < swapchain.images.size(); i++){
        present_complete_semaphores.emplace_back(vk::raii::Semaphore(logical_device, vk::SemaphoreCreateInfo()));
        render_finished_semaphores.emplace_back(vk::raii::Semaphore(logical_device, vk::SemaphoreCreateInfo()));
    }

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        in_flight_fences.emplace_back(vk::raii::Fence(logical_device, {vk::FenceCreateFlagBits::eSignaled}));
    }
}

// ------ Render Loop Functions

void Engine::run(){
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        drawFrame();
    }

    logical_device.waitIdle();

    cleanup();
}

void Engine::recordCommandBuffer(uint32_t image_index){
    graphics_command_buffer[current_frame].begin({});

    transparent_draws.clear();

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
    vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);
    vk::RenderingAttachmentInfo color_attachment_info{};
    color_attachment_info.imageView = color_image.image_view;
    color_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment_info.storeOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment_info.clearValue = clear_color;

    color_attachment_info.resolveMode = vk::ResolveModeFlagBits::eAverage;
    color_attachment_info.resolveImageView = swapchain.image_views[image_index];
    color_attachment_info.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::RenderingAttachmentInfo depth_attachment_info = {};
    depth_attachment_info.imageView = depth_image.image_view;
    depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment_info.storeOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment_info.clearValue = clear_depth;

    vk::RenderingInfo rendering_info;
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = swapchain.extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment_info;
    rendering_info.pDepthAttachment = &depth_attachment_info;

    graphics_command_buffer[current_frame].beginRendering(rendering_info);

    graphics_command_buffer[current_frame].setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.f, 1.f));
    graphics_command_buffer[current_frame].setScissor(0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapchain.extent));
    /**
     * vertexCount -> number of vertices
     * instanceCount -> used for instanced rendering, use 1 if not using it
     * firstVertex -> used as an offset into the vertex buffer, defines the lowest value of SV_VertexId
     * firstInstance -> used as an offset for instanced rendering, defines the lowest value of SV_InstanceID
     */
    //command_buffers[current_frame].draw(vertices.size(), 1, 0, 0);
    // graphics_command_buffer[current_frame].drawIndexed(indices.size(), 1, 0, 0, 0);

    glm::vec3 camera_pos = camera.getCurrentState().f_camera.position;

    for(auto& obj : scene_objs){
        if (!obj.t_primitives.empty()) {
            for(const auto& primitive : obj.t_primitives) {
                glm::vec3 prim_world_center = glm::vec3(obj.model_matrix * glm::vec4(primitive.center, 1.0f));
                float distance_sq = glm::distance2(camera_pos, prim_world_center);
                
                const Material& material = obj.materials[primitive.material_index];
                transparent_draws.push_back({&obj, &primitive, &material, distance_sq});
            }
        }


        // Bind the geometry buffers ONCE per object
        graphics_command_buffer[current_frame].bindVertexBuffers(0, {obj.geometry_buffer.buffer}, {0});
        graphics_command_buffer[current_frame].bindIndexBuffer(obj.geometry_buffer.buffer, obj.index_buffer_offset, vk::IndexType::eUint32);

        graphics_command_buffer[current_frame].bindPipeline(vk::PipelineBindPoint::eGraphics, *obj.o_pipeline->pipeline);    
        graphics_command_buffer[current_frame].pushConstants<glm::mat4>(*obj.o_pipeline -> layout, vk::ShaderStageFlagBits::eVertex, 0, obj.model_matrix);

        for(auto& primitive : obj.o_primitives){
            // Get the material for this primitive
            const Material& material = obj.materials[primitive.material_index];

            MaterialPushConstant mat_constants;
            mat_constants.base_color_factor = material.base_color_factor;
            mat_constants.metallic_factor = material.metallic_factor;
            mat_constants.roughness_factor = material.roughness_factor;
            mat_constants.emissive_factor_and_pad = glm::vec4(material.emissive_factor, 1.f);
            mat_constants.occlusion_strength = material.occlusion_strength;
            mat_constants.specular_factor = material.specular_factor;
            mat_constants.specular_color_factor = material.specular_color_factor;
            // mat_constants.clearcoat_factor = material.clearcoat_factor; <-- REMOVED
            // mat_constants.clearcoat_roughness_factor = material.clearcoat_roughness_factor; <-- REMOVED

            graphics_command_buffer[current_frame].pushConstants<MaterialPushConstant>(
                *obj.o_pipeline -> layout, 
                vk::ShaderStageFlagBits::eFragment, 
                sizeof(glm::mat4), // The offset we defined
                mat_constants
            );

            // Bind the material's descriptor set
            graphics_command_buffer[current_frame].bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, 
                *obj.o_pipeline -> layout, 0, 
                *material.descriptor_sets[current_frame], 
                nullptr);
            
            // Draw the primitive
            graphics_command_buffer[current_frame].drawIndexed(
                primitive.index_count,   // indexCount
                1,                      // instanceCount
                primitive.first_index,   // firstIndex
                0,                      // vertexOffset
                0);                     // firstInstance
        }
    }

    std::sort(transparent_draws.begin(), transparent_draws.end());
    if (!transparent_draws.empty()) {
        Gameobject* last_bound_object = nullptr;
        PipelineInfo* last_bound_pipeline = nullptr;

        for (const auto& draw : transparent_draws) {
            Gameobject* obj = draw.object;
            const Primitive& primitive = *draw.primitive;
            const Material& material = *draw.material;
            PipelineInfo* pipeline = obj->t_pipeline; // Get object's transparent pipeline

            // --- A. Bind Pipeline (if changed) ---
            if (pipeline != last_bound_pipeline) {
                graphics_command_buffer[current_frame].bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline);
                last_bound_pipeline = pipeline;
            }

            // --- B. Bind Geometry & Model Matrix (if changed) ---
            if (obj != last_bound_object) {
                graphics_command_buffer[current_frame].bindVertexBuffers(0, {obj->geometry_buffer.buffer}, {0});
                graphics_command_buffer[current_frame].bindIndexBuffer(obj->geometry_buffer.buffer, obj->index_buffer_offset, vk::IndexType::eUint32);
                graphics_command_buffer[current_frame].pushConstants<glm::mat4>(*pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, obj->model_matrix);
                last_bound_object = obj;
            } 
            // Also push matrix if pipeline changed but object didn't
            else if (pipeline != last_bound_pipeline) {
                graphics_command_buffer[current_frame].pushConstants<glm::mat4>(*pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, obj->model_matrix);
            }

            // --- C. Bind Material & Draw ---
            MaterialPushConstant mat_constants;
            mat_constants.base_color_factor = material.base_color_factor;
            mat_constants.metallic_factor = material.metallic_factor;
            mat_constants.roughness_factor = material.roughness_factor;
            mat_constants.emissive_factor_and_pad = glm::vec4(material.emissive_factor, 1.f);
            mat_constants.occlusion_strength = material.occlusion_strength;
            mat_constants.specular_factor = material.specular_factor;
            mat_constants.specular_color_factor = material.specular_color_factor;
            // mat_constants.clearcoat_factor = material.clearcoat_factor; <-- REMOVED
            // mat_constants.clearcoat_roughness_factor = material.clearcoat_roughness_factor; <-- REMOVED

            graphics_command_buffer[current_frame].pushConstants<MaterialPushConstant>(
                *pipeline->layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), mat_constants
            );

            graphics_command_buffer[current_frame].bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, *pipeline->layout, 0, *material.descriptor_sets[current_frame], nullptr
            );
            
            graphics_command_buffer[current_frame].drawIndexed(
                primitive.index_count, 1, primitive.first_index, 0, 0
            );
        }
    }

    
    // --- MODIFIED: Torus drawing (must also use primitive loop) ---
    /* graphics_command_buffer[current_frame].bindPipeline(vk::PipelineBindPoint::eGraphics, *torus.pipeline->pipeline);
    glm::mat4 model_toroid = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)); // TO FIX
    graphics_command_buffer[current_frame].pushConstants<glm::mat4>(*torus.pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, model_toroid);

    // Bind torus geometry
    graphics_command_buffer[current_frame].bindVertexBuffers(0, {torus.geometry_buffer.buffer}, {0});
    graphics_command_buffer[current_frame].bindIndexBuffer(torus.geometry_buffer.buffer, torus.index_buffer_offset, vk::IndexType::eUint32);

    // Loop over torus primitives (even if there's only one)
    for (const auto& primitive : torus.primitives) {
        const Material& material = torus.materials[primitive.material_index];
        graphics_command_buffer[current_frame].bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, 
            *torus.pipeline -> layout, 0, 
            *material.descriptor_sets[current_frame], 
            nullptr);
            
        graphics_command_buffer[current_frame].drawIndexed(primitive.index_count, 1, primitive.first_index, 0, 0);
    } */
    graphics_command_buffer[current_frame].endRendering();

    transition_image_layout(
        image_index,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe // a "sink" stage that ensures all prior work is finished before transitioning
    );

    graphics_command_buffer[current_frame].end();
}

void Engine::drawFrame(){
    while( vk::Result::eTimeout == logical_device.waitForFences(*in_flight_fences[current_frame], vk::True, UINT64_MAX));
    
    if (framebuffer_resized) {
        framebuffer_resized = false;
        recreateSwapChain();
    }

    // Waiting for the previous frame to complete
    auto [result, image_index] = swapchain.swapchain.acquireNextImage(UINT64_MAX, *present_complete_semaphores[semaphore_index], nullptr);

    if(result == vk::Result::eErrorOutOfDateKHR){
        framebuffer_resized = false;
        recreateSwapChain();
        return;
    }
    if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR){
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    logical_device.resetFences(*in_flight_fences[current_frame]);
    graphics_command_buffer[current_frame].reset();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - prev_time).count();

    bool changed = torus.inputUpdate(input, time);
    if(changed){
        createModel(torus); // TODO 0001 -> maybe there is a better way to manage this
    }
    camera.update(time, input, torus.getRadius(), torus.getHeight());

    updateUniformBuffer(current_frame);

    recordCommandBuffer(image_index);

    prev_time = current_time;

    vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit_info;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &*present_complete_semaphores[semaphore_index];
    submit_info.pWaitDstStageMask = &wait_destination_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &*graphics_command_buffer[current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &*render_finished_semaphores[image_index];

    graphics_queue.submit(submit_info, *in_flight_fences[current_frame]);

    vk::PresentInfoKHR present_info_KHR;
    present_info_KHR.waitSemaphoreCount = 1;
    present_info_KHR.pWaitSemaphores = &*render_finished_semaphores[image_index];
    present_info_KHR.swapchainCount = 1;
    present_info_KHR.pSwapchains = &*swapchain.swapchain;
    present_info_KHR.pImageIndices = &image_index;

    result = present_queue.presentKHR(present_info_KHR);
    switch(result){
        case vk::Result::eSuccess: break;
        case vk::Result::eSuboptimalKHR:
            std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
            break;
        default: break; // an unexpected result is returned
    }

    total_elapsed += time;
    fps_count++;
    if (total_elapsed >= 1.f) { // If one second has passed
        double fps = static_cast<double>(fps_count)/(total_elapsed);

        std::string title = "Vulkan Engine - " +  std::to_string(static_cast<int>(round(fps))) + " FPS";
        glfwSetWindowTitle(window, title.c_str());

        total_elapsed = 0.f;
        fps_count = 0.f;
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    semaphore_index = (semaphore_index + 1) % present_complete_semaphores.size();
}

void Engine::updateUniformBuffer(uint32_t current_image)
{
    UniformBufferObject ubo{};
    // ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(1.f));
    // ubo.model = glm::rotate(ubo.model, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));

    ubo.view = camera.getViewMatrix();

    ubo.proj = camera.getProjectionMatrix();

    ubo.camera_pos = camera.getCurrentState().f_camera.position;

    memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
}

void Engine::recreateSwapChain(){
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0){
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    logical_device.waitIdle();

    swapchain.image_views.clear();
    swapchain.swapchain = nullptr;
    

    swapchain = Swapchain::createSwapChain(*this);

    // vkDestroyImageView(*logical_device, depth_image.image_view, nullptr);
    vmaDestroyImage(vma_allocator, depth_image.image, depth_image.allocation);
    Image::createDepthResources(physical_device, depth_image, swapchain.extent.width, swapchain.extent.height, *this);

}

// ------ Closing functions

void Engine::cleanup(){
    // Destroying sync objects
    in_flight_fences.clear();
    render_finished_semaphores.clear();
    present_complete_semaphores.clear();


    // Destroying sync objects
    graphics_command_buffer.clear();
    command_pool_graphics = nullptr;
    command_pool_transfer = nullptr;

    // Destroying uniform buffers objects
    /* for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vmaUnmapMemory(vma_allocator, uniform_buffers[i].allocation);
        // vmaDestroyBuffer(vma_allocator,uniform_buffers[i].buffer, uniform_buffers[i].allocation);

        descriptor_sets[i] = nullptr;
    }
    descriptor_pool = nullptr; */

    // Destroying vertex/index data
    // vmaDestroyBuffer(vma_allocator,data_buffer.buffer, data_buffer.allocation);

    // Delete pipeline objs
    

    // Delete swapchain
    swapchain.image_views.clear();
    swapchain.swapchain = nullptr;

    // Destroying the allocator
    vmaDestroyAllocator(vma_allocator);

    // Delete instance and windowig
    surface = nullptr;
    debug_messanger = nullptr;
    instance = nullptr;
    context = {};


    // Destroy window
    glfwDestroyWindow(window);
    glfwTerminate();
}

