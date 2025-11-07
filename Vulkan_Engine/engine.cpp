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
void Engine::printGpuMemoryUsage()
{
    // Get memory properties from the physical device
    vk::PhysicalDeviceMemoryProperties mem_properties = physical_device.getMemoryProperties();

    // VmaBudget array to hold data for each heap
    std::vector<VmaBudget> budgets(mem_properties.memoryHeapCount);
    
    // Get the budgets
    vmaGetHeapBudgets(vma_allocator, budgets.data());

    std::cout << "--- GPU Memory Usage ---" << std::endl;
    for(uint32_t i = 0; i < mem_properties.memoryHeapCount; ++i)
    {
        // Convert bytes to MiB
        double usage_mib = static_cast<double>(budgets[i].usage) / (1024.0 * 1024.0);
        double budget_mib = static_cast<double>(budgets[i].budget) / (1024.0 * 1024.0);
        
        std::cout << "Heap " << i << ": ";
        if(mem_properties.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
            std::cout << "[VRAM] ";
        } else {
            std::cout << "[System RAM] ";
        }
        
        std::cout.precision(2);
        std::cout << std::fixed << usage_mib << " MiB used / " 
                  << std::fixed << budget_mib << " MiB budget" << std::endl;
    }
    std::cout << "------------------------" << std::endl;
}



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
    // We need 24 vertices (4 vertices per face * 6 faces)
    cube.vertices.resize(24);

    // --- Define vertices PER FACE with correct normals ---
    // Front face (Z = -0.5)
    cube.vertices[0].pos = {-0.5f, -0.5f, -0.5f}; cube.vertices[0].normal = {0.0f, 0.0f, -1.0f};
    cube.vertices[1].pos = { 0.5f, -0.5f, -0.5f}; cube.vertices[1].normal = {0.0f, 0.0f, -1.0f};
    cube.vertices[2].pos = { 0.5f,  0.5f, -0.5f}; cube.vertices[2].normal = {0.0f, 0.0f, -1.0f};
    cube.vertices[3].pos = {-0.5f,  0.5f, -0.5f}; cube.vertices[3].normal = {0.0f, 0.0f, -1.0f};
    // Back face (Z = 0.5)
    cube.vertices[4].pos = {-0.5f, -0.5f,  0.5f}; cube.vertices[4].normal = {0.0f, 0.0f, 1.0f};
    cube.vertices[5].pos = { 0.5f, -0.5f,  0.5f}; cube.vertices[5].normal = {0.0f, 0.0f, 1.0f};
    cube.vertices[6].pos = { 0.5f,  0.5f,  0.5f}; cube.vertices[6].normal = {0.0f, 0.0f, 1.0f};
    cube.vertices[7].pos = {-0.5f,  0.5f,  0.5f}; cube.vertices[7].normal = {0.0f, 0.0f, 1.0f};
    // Left face (X = -0.5)
    cube.vertices[8].pos = {-0.5f, -0.5f,  0.5f}; cube.vertices[8].normal = {-1.0f, 0.0f, 0.0f};
    cube.vertices[9].pos = {-0.5f, -0.5f, -0.5f}; cube.vertices[9].normal = {-1.0f, 0.0f, 0.0f};
    cube.vertices[10].pos = {-0.5f,  0.5f, -0.5f}; cube.vertices[10].normal = {-1.0f, 0.0f, 0.0f};
    cube.vertices[11].pos = {-0.5f,  0.5f,  0.5f}; cube.vertices[11].normal = {-1.0f, 0.0f, 0.0f};
    // Right face (X = 0.5)
    cube.vertices[12].pos = { 0.5f, -0.5f, -0.5f}; cube.vertices[12].normal = {1.0f, 0.0f, 0.0f};
    cube.vertices[13].pos = { 0.5f, -0.5f,  0.5f}; cube.vertices[13].normal = {1.0f, 0.0f, 0.0f};
    cube.vertices[14].pos = { 0.5f,  0.5f,  0.5f}; cube.vertices[14].normal = {1.0f, 0.0f, 0.0f};
    cube.vertices[15].pos = { 0.5f,  0.5f, -0.5f}; cube.vertices[15].normal = {1.0f, 0.0f, 0.0f};
    // Top face (Y = 0.5)
    cube.vertices[16].pos = {-0.5f,  0.5f, -0.5f}; cube.vertices[16].normal = {0.0f, 1.0f, 0.0f};
    cube.vertices[17].pos = { 0.5f,  0.5f, -0.5f}; cube.vertices[17].normal = {0.0f, 1.0f, 0.0f};
    cube.vertices[18].pos = { 0.5f,  0.5f,  0.5f}; cube.vertices[18].normal = {0.0f, 1.0f, 0.0f};
    cube.vertices[19].pos = {-0.5f,  0.5f,  0.5f}; cube.vertices[19].normal = {0.0f, 1.0f, 0.0f};
    // Bottom face (Y = -0.5)
    cube.vertices[20].pos = {-0.5f, -0.5f, -0.5f}; cube.vertices[20].normal = {0.0f, -1.0f, 0.0f};
    cube.vertices[21].pos = { 0.5f, -0.5f, -0.5f}; cube.vertices[21].normal = {0.0f, -1.0f, 0.0f};
    cube.vertices[22].pos = { 0.5f, -0.5f,  0.5f}; cube.vertices[22].normal = {0.0f, -1.0f, 0.0f};
    cube.vertices[23].pos = {-0.5f, -0.5f,  0.5f}; cube.vertices[23].normal = {0.0f, -1.0f, 0.0f};

    // Set default values for other attributes
    for(int i = 0; i < 24; ++i) {
        auto& v = cube.vertices[i];
        v.color = {1.0f, 1.0f, 1.0f};
        // Simple UV mapping for a cube
        if (v.normal.z != 0.0f) { // Front/Back
            v.tex_coord = {v.pos.x + 0.5f, v.pos.y + 0.5f};
        } else if (v.normal.x != 0.0f) { // Left/Right
            v.tex_coord = {v.pos.z + 0.5f, v.pos.y + 0.5f};
        } else { // Top/Bottom
            v.tex_coord = {v.pos.x + 0.5f, v.pos.z + 0.5f};
        }
        v.tex_coord_1 = v.tex_coord;
        // Simple tangent
        if (abs(v.normal.y) > 0.9f) { // Top/Bottom
             v.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
        } else { // Sides
             v.tangent = {0.0f, 1.0f, 0.0f, 1.0f};
        }
    }

    // Indices are now sequential for each face
    cube.indices = {
        // front
        0, 1, 2, 2, 3, 0,
        // back
        4, 5, 6, 6, 7, 4,
        // left
        8, 9, 10, 10, 11, 8,
        // right
        12, 13, 14, 14, 15, 12,
        // top
        16, 17, 18, 18, 19, 16,
        // bottom
        20, 21, 22, 22, 23, 20
    };

    // --- Material and Primitive setup remains the same ---
    Material mat;
    mat.albedo_texture_index = -1; 
    mat.base_color_factor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); 
    mat.emissive_factor = glm::vec3(1.0f, 1.0f, 0.0f); 
    mat.metallic_factor = 0.0f;
    mat.roughness_factor = 1.0f;
    mat.is_transparent = false;
    cube.materials.push_back(std::move(mat));

    Primitive prim;
    prim.index_count = static_cast<uint32_t>(cube.indices.size());
    prim.first_index = 0;
    prim.material_index = 0; 
    prim.center = glm::vec3(0.0f, 0.0f, 0.0f);
    cube.o_primitives.push_back(prim); 

    cube.textures.emplace_back();
    cube.default_sampler = Image::createTextureSampler(physical_device, &logical_device, 1);
    cube.createDefaultTexture(*this, cube.textures[0], glm::vec4(1.0, 1.0, 1.0, 1.0));

    createModel(cube);
    return cube;
}

void Engine::createRTBox(const std::string& rtbox_path)
{
    std::ifstream rtbox_file(rtbox_path);
    if (!rtbox_file.is_open()) {
        std::cerr << "Warning: Failed to open rtbox file: " << rtbox_path << std::endl;
        return;
    }
    json config = json::parse(rtbox_file);

    // --- 1. Parse Config ---
    glm::vec3 pos = {
        config["position"][0].get<float>(),
        config["position"][1].get<float>(),
        config["position"][2].get<float>()
    };
    glm::vec3 dim = {
        config["dimensions"][0].get<float>(),
        config["dimensions"][1].get<float>(),
        config["dimensions"][2].get<float>()
    };

    // Half dimensions for easier vertex placement
    float w = dim.x / 2.0f;
    float h = dim.y; // Height is full dimension
    float d = dim.z / 2.0f;
    float y_bot = pos.y;
    float y_top = pos.y + h;

    rt_box.vertices.resize(20); // 5 faces * 4 vertices
    rt_box.indices.resize(30);  // 5 faces * 6 indices (2 tris)

    // --- 2. Define Vertices (using pos and dim) ---
    // Floor (Y = y_bot)
    rt_box.vertices[0].pos = {pos.x - w, y_bot, pos.z - d}; rt_box.vertices[0].normal = {0, 1, 0};
    rt_box.vertices[1].pos = {pos.x + w, y_bot, pos.z - d}; rt_box.vertices[1].normal = {0, 1, 0};
    rt_box.vertices[2].pos = {pos.x + w, y_bot, pos.z + d}; rt_box.vertices[2].normal = {0, 1, 0};
    rt_box.vertices[3].pos = {pos.x - w, y_bot, pos.z + d}; rt_box.vertices[3].normal = {0, 1, 0};
    // Ceiling (Y = y_top)
    rt_box.vertices[4].pos = {pos.x - w, y_top, pos.z - d}; rt_box.vertices[4].normal = {0, -1, 0};
    rt_box.vertices[5].pos = {pos.x + w, y_top, pos.z - d}; rt_box.vertices[5].normal = {0, -1, 0};
    rt_box.vertices[6].pos = {pos.x + w, y_top, pos.z + d}; rt_box.vertices[6].normal = {0, -1, 0};
    rt_box.vertices[7].pos = {pos.x - w, y_top, pos.z + d}; rt_box.vertices[7].normal = {0, -1, 0};
    // Back Wall (Z = pos.z - d)
    rt_box.vertices[8].pos = {pos.x - w, y_bot, pos.z - d}; rt_box.vertices[8].normal = {0, 0, 1};
    rt_box.vertices[9].pos = {pos.x + w, y_bot, pos.z - d}; rt_box.vertices[9].normal = {0, 0, 1};
    rt_box.vertices[10].pos = {pos.x + w, y_top, pos.z - d}; rt_box.vertices[10].normal = {0, 0, 1};
    rt_box.vertices[11].pos = {pos.x - w, y_top, pos.z - d}; rt_box.vertices[11].normal = {0, 0, 1};
    // Left Wall (X = pos.x - w)
    rt_box.vertices[12].pos = {pos.x - w, y_bot, pos.z + d}; rt_box.vertices[12].normal = {1, 0, 0};
    rt_box.vertices[13].pos = {pos.x - w, y_bot, pos.z - d}; rt_box.vertices[13].normal = {1, 0, 0};
    rt_box.vertices[14].pos = {pos.x - w, y_top, pos.z - d}; rt_box.vertices[14].normal = {1, 0, 0};
    rt_box.vertices[15].pos = {pos.x - w, y_top, pos.z + d}; rt_box.vertices[15].normal = {1, 0, 0};
    // Right Wall (X = pos.x + w)
    rt_box.vertices[16].pos = {pos.x + w, y_bot, pos.z - d}; rt_box.vertices[16].normal = {-1, 0, 0};
    rt_box.vertices[17].pos = {pos.x + w, y_bot, pos.z + d}; rt_box.vertices[17].normal = {-1, 0, 0};
    rt_box.vertices[18].pos = {pos.x + w, y_top, pos.z + d}; rt_box.vertices[18].normal = {-1, 0, 0};
    rt_box.vertices[19].pos = {pos.x + w, y_top, pos.z - d}; rt_box.vertices[19].normal = {-1, 0, 0};
    
    for(auto& v : rt_box.vertices) {
        v.color = {1.f, 1.f, 1.f};
        v.tex_coord = {0.f, 0.f};
        v.tex_coord_1 = {0.f, 0.f};
        v.tangent = {1.f, 0.f, 0.f, 0.f}; // W = 0 for no normal map
    }

    // Indices (5 faces * 6 indices)
    rt_box.indices = {
        0, 3, 2, 2, 1, 0,   // Floor
        4, 5, 6, 6, 7, 4,   // Ceiling
        8, 9, 10, 10, 11, 8, // Back
        12, 13, 14, 14, 15, 12, // Left
        16, 17, 18, 18, 19, 16  // Right
    };
    
    // --- 3. Helper to create materials from JSON ---
    auto createMatFromJson = [&](const json& mat_config) -> Material {
        Material mat;
        mat.base_color_factor = glm::vec4(
            1.0f
        );
        mat.metallic_factor = mat_config.value("metallic", 0.0f);
        mat.roughness_factor = mat_config.value("roughness", 1.0f);
        mat.emissive_factor = glm::vec3(0.0f); // All materials are non-emissive
        mat.albedo_texture_index = -1;
        mat.is_transparent = false;
        return mat;
    };

    // --- 4. Create Materials, Primitives, and Lights ---
    
    // Clear old data
    rt_box.materials.clear();
    rt_box.o_primitives.clear();
    panel_lights.clear();
    panel_lights_on.clear();

    std::vector<std::string> panel_names = {"floor", "ceiling", "back_wall", "left_wall", "right_wall"};
    std::vector<glm::vec4> light_positions = {
        glm::vec4(pos.x, y_bot + 1.f, pos.z, 0.0f),     // Floor light
        glm::vec4(pos.x, y_top - 1.f, pos.z, 0.0f),     // Ceiling light
        glm::vec4(pos.x, pos.y + h/2.0f, pos.z - d + 1.f, 0.0f), // Back light
        glm::vec4(pos.x - w + 1.f, pos.y + h/2.0f, pos.z, 0.0f), // Left light
        glm::vec4(pos.x + w - 1.f, pos.y + h/2.0f, pos.z, 0.0f)  // Right light
    };

    for (int i = 0; i < 5; ++i) {
        const std::string& name = panel_names[i];
        const json& panel_config = config["panels"][name];

        // Create material
        rt_box.materials.push_back(createMatFromJson(panel_config["material"]));
        
        // Create primitive (indices are 6 per face, starting at 0, 6, 12, etc.)
        rt_box.o_primitives.push_back({
            static_cast<uint32_t>(i * 6), // first_index
            6,                            // index_count
            i                             // material_index
        });

        // Create light
        float intensity = panel_config["light"].value("intensity", 0.0f);
        glm::vec3 color = {
            panel_config["material"]["base_color"][0].get<float>(),
            panel_config["material"]["base_color"][1].get<float>(),
            panel_config["material"]["base_color"][2].get<float>()
        };

        Pointlight light;
        light.position = light_positions[i];
        light.color = glm::vec4(color, intensity);
        panel_lights.push_back(light);
        
        // Add toggle state
        panel_lights_on.push_back(false); // Default to OFF
    }

    // --- 5. GPU Resources ---
    rt_box.textures.emplace_back();
    rt_box.default_sampler = Image::createTextureSampler(physical_device, &logical_device, 1);
    rt_box.createDefaultTexture(*this, rt_box.textures[0], glm::vec4(1.0, 1.0, 1.0, 1.0));
    createModel(rt_box); // Upload to GPU
}

// Simple barrier helper
void Engine::transitionImage(vk::raii::CommandBuffer& cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspectMask) {
    vk::ImageMemoryBarrier2 barrier;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Set masks based on layout
    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal || newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        if (aspectMask == vk::ImageAspectFlagBits::eDepth) {
            // This is a cube map, so transition all 6 layers
            barrier.subresourceRange.layerCount = 6;
        }
    }

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = {}; // No source access needed
        barrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    }else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal && newLayout == vk::ImageLayout::eTransferSrcOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    }
    else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
    } 
    else if (oldLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

    }
    else {
        // Generic (less optimal)
        barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    }

    vk::DependencyInfo dependency_info;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;
    cmd.pipelineBarrier2(dependency_info);
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
        
        case Action::DEBUG_LIGHTS: 
            if (action == GLFW_PRESS)
                engine->debug_lights = !engine->debug_lights; 
            break;
            
        case Action::TOGGLE_FLOOR_LIGHT:
            if (action == GLFW_PRESS && !engine->panel_lights_on.empty())
                engine->panel_lights_on[0] = !engine->panel_lights_on[0];
            break;
        case Action::TOGGLE_CEILING_LIGHT:
            if (action == GLFW_PRESS && engine->panel_lights_on.size() > 1)
                engine->panel_lights_on[1] = !engine->panel_lights_on[1];
            break;
        case Action::TOGGLE_BACK_LIGHT:
            if (action == GLFW_PRESS && engine->panel_lights_on.size() > 2)
                engine->panel_lights_on[2] = !engine->panel_lights_on[2];
            break;
        case Action::TOGGLE_LEFT_LIGHT:
            if (action == GLFW_PRESS && engine->panel_lights_on.size() > 3)
                engine->panel_lights_on[3] = !engine->panel_lights_on[3];
            break;
        case Action::TOGGLE_RIGHT_LIGHT:
            if (action == GLFW_PRESS && engine->panel_lights_on.size() > 4)
                engine->panel_lights_on[4] = !engine->panel_lights_on[4];
            break;
        
        case Action::TOGGLE_EMISSIVE:
            if (action == GLFW_PRESS)
                engine->use_emissive_lights = !engine->use_emissive_lights;
            break;
        case Action::TOGGLE_MANUAL:
            if (action == GLFW_PRESS)
                engine->use_manual_lights = !engine->use_manual_lights;
            break;
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
    // mssa_samples = vk::SampleCountFlagBits::e4; // HARD-CODED FOR FULLSCREEN
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
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                           VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    VkResult res = vmaCreateAllocator(&allocator_info, &vma_allocator);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateAllocator failed");
    }
    
    std::cout << "Memory status after creation" << std::endl;
    printGpuMemoryUsage();

    // Command creation
    createCommandPool();

    // TO FIX THIS
    color_image = Image::createImage(swapchain.extent.width, swapchain.extent.height,
                        1, mssa_samples, swapchain.format, vk::ImageTiling::eOptimal, 
                    vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, *this);
    color_image.image_view = Image::createImageView(color_image, *this);
    std::cout << "Memory usage color image creation" << std::endl;
    printGpuMemoryUsage();
    Image::createDepthResources(physical_device, depth_image, swapchain.extent.width, swapchain.extent.height, *this);
    std::cout << "Memory usage after depth image creation" << std::endl;
    printGpuMemoryUsage();
    createOITResources();
    std::cout << "Memory usage after OIT resources creation" << std::endl;
    printGpuMemoryUsage();

    createShadowResources();

    // PIPELINE CREATION
    createPipelines();

    loadObjects("resources/scene.json");
    std::cout << "Memory status loading objects in scene" << std::endl;
    printGpuMemoryUsage();

    debug_cube = createDebugCube();
    
    // Assign the opaque PBR pipeline to the debug cube
    PipelineKey p_key;
    p_key.v_shader = v_shader_pbr;
    p_key.f_shader = f_shader_pbr;
    p_key.mode = TransparencyMode::OPAQUE;
    p_key.cull_mode = vk::CullModeFlagBits::eBack;
    debug_cube.o_pipeline = &p_p_map[p_key];


    createTorusModel();

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
        // Resize the inner vectors to hold 5 elements
        shadow_ubos[i].resize(MAX_PANEL_LIGHTS);
        shadow_ubos_mapped[i].resize(MAX_PANEL_LIGHTS);

        for (int j = 0; j < MAX_PANEL_LIGHTS; ++j) {
            createBuffer(vma_allocator, sizeof(ShadowUBO), 
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                         shadow_ubos[i][j]); // This access is now valid
            vmaMapMemory(vma_allocator, shadow_ubos[i][j].allocation, &shadow_ubos_mapped[i][j]);
        }
    }

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createOITDescriptorSets();

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
        // Binding 6: Transmission
        vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eCombinedImageSampler, 1,
                                        vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 7: Clearcoat
        vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eCombinedImageSampler, 1,
                                        vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 8: Clearcoat Roughness
        vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eCombinedImageSampler, 1,
                                        vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 9: Shadow Map Array
        vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eCombinedImageSampler, 
                                       MAX_PANEL_LIGHTS, // 5 maps
                                       vk::ShaderStageFlagBits::eFragment, nullptr)
    };

    createShadowPipeline();
    
    // Key definition
    p_key.v_shader = v_shader_pbr;
    p_key.f_shader = f_shader_pbr;
    p_key.mode = TransparencyMode::OPAQUE;
    p_key.cull_mode = vk::CullModeFlagBits::eBack;

    // Pipeline creation
    PipelineInfo& p_info = p_p_map[p_key];
    p_info.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    p_info.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &p_info,
        p_key.v_shader,
        p_key.f_shader,
        p_key.mode,
        p_key.cull_mode
    );

    createOITCompositePipeline();

    // OIT Write PBR pipeline
    p_key.f_shader = f_shader_oit_write;
    p_key.mode = TransparencyMode::OIT_WRITE;
    PipelineInfo& p_info_transparent = p_p_map[p_key];
    p_info_transparent.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    p_info_transparent.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &p_info_transparent,
        p_key.v_shader,
        p_key.f_shader,
        p_key.mode,
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
    p_key.mode = TransparencyMode::OIT_WRITE;
    p_key.cull_mode = vk::CullModeFlagBits::eBack;

    PipelineInfo& p_info_torus = p_p_map[p_key];
    p_info_torus.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    p_info_torus.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &p_info_torus,
        p_key.v_shader,
        p_key.f_shader,
        p_key.mode,
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

    // --- 1. Parse Settings ---
    float emissive_multiplier = 1.0f;
    std::string lights_path = "";
    std::string rtbox_path = "";

    if (scene_data.contains("settings")) {
        const auto& settings = scene_data["settings"];
        this->use_manual_lights = settings.value("use_manual_lights", false);
        this->use_emissive_lights = settings.value("use_emissive_lights", false);
        emissive_multiplier = settings.value("emissive_intensity_multiplier", 1.0f);
        lights_path = settings.value("lights_file", "");
        this->use_rt_box = settings.value("use_rt_box", false);
        rtbox_path = settings.value("rt_box_file", "");

        this->panel_shadows_enabled = settings.value("panel_shadows_enabled", true);
        this->shadow_light_far_plane = settings.value("shadow_far_plane", 100.0f);
    }

    // --- 2. Load Manual Lights ---
    if (use_manual_lights && !lights_path.empty()) {
        loadManualLights(lights_path);
    } else if (use_manual_lights && lights_path.empty()) {
        std::cerr << "Warning: 'use_manual_lights' is true but no 'lights_file' was specified." << std::endl;
    }

    // --- 3. Load Objects ---
    if (!scene_data.contains("objects")) {
        std::cout << "Warning: Scene file contains no 'objects' array." << std::endl;
        return;
    }

    // Iterate over each object definition in the JSON
    emissive_lights.clear();
    for (const auto& obj_def : scene_data["objects"]) {
        P_object new_object;

        std::string model_path = obj_def["model"];
        new_object.loadModel(model_path, *this);
        
        createModel(new_object);

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

        // --- (Pipeline assignment logic - unchanged) ---
        PipelineKey p_key;
        p_key.v_shader = "shaders/basic/vertex.spv";
        p_key.f_shader = "shaders/basic/fragment.spv";
        p_key.mode = TransparencyMode::OPAQUE;
        p_key.cull_mode = vk::CullModeFlagBits::eBack;
        if(!p_p_map.contains(p_key)){ /* ... error ... */ }
        new_object.o_pipeline = &p_p_map[p_key];
        p_key.mode = TransparencyMode::OIT_WRITE;
        p_key.f_shader = f_shader_oit_write;
        new_object.t_pipeline = &p_p_map[p_key];
        // --- (End pipeline logic) ---

        // --- 4. Load Emissive Lights (if enabled) ---
        if (use_emissive_lights) {
            auto new_lights = new_object.createEmissiveLights(emissive_multiplier);
            // Append new lights to our master list
            emissive_lights.insert(emissive_lights.end(), new_lights.begin(), new_lights.end());
        }

        if (this->use_rt_box && !rtbox_path.empty()) {
            createRTBox(rtbox_path); // Create the box
            
            // --- We must assign its pipelines here! ---
            PipelineKey p_key;
            p_key.v_shader = v_shader_pbr;
            p_key.f_shader = f_shader_pbr;
            p_key.mode = TransparencyMode::OPAQUE;
            p_key.cull_mode = vk::CullModeFlagBits::eBack;
            rt_box.o_pipeline = &p_p_map[p_key];

            PipelineKey p_key_trans = p_key;
            p_key_trans.f_shader = f_shader_oit_write;
            p_key_trans.mode = TransparencyMode::OIT_WRITE;
            rt_box.t_pipeline = &p_p_map[p_key_trans];
            // --- End pipeline assignment ---
            
        } else if (this->use_rt_box && rtbox_path.empty()) {
            std::cerr << "Warning: 'use_rt_box' is true but no 'rt_box_file' was specified." << std::endl;
        }

        scene_objs.emplace_back(std::move(new_object));
    }
}


void Engine::loadManualLights(const std::string& lights_path)
{
    std::ifstream lights_file(lights_path);
    if (!lights_file.is_open()) {
        std::cerr << "Warning: Failed to open lights file: " << lights_path << std::endl;
        return;
    }
    
    json lights_data = json::parse(lights_file);

    manual_lights.clear();

    for (const auto& light_def : lights_data) {
        // Stop if we've filled the UBO
        if (ubo.curr_num_pointlights >= MAX_POINTLIGHTS) {
            std::cerr << "Warning: Reached maximum point lights (" << MAX_POINTLIGHTS 
                      << "). Skipping remaining lights in file." << std::endl;
            break;
        }

        std::string type = light_def.value("type", "pointlight");

        if (type == "pointlight") {
            int light_index = ubo.curr_num_pointlights;

            // Get position
            glm::vec3 pos(0.0f);
            if (light_def.contains("position")) {
                pos = {light_def["position"][0], light_def["position"][1], light_def["position"][2]};
            }
            
            // Get color
            glm::vec3 color(1.0f);
            if (light_def.contains("color")) {
                color = {light_def["color"][0], light_def["color"][1], light_def["color"][2]};
            }

            // Get intensity
            float intensity = light_def.value("intensity", 1.0f);

            Pointlight new_light;
            new_light.position = glm::vec4(pos, 0.0f);
            new_light.color = glm::vec4(color, intensity);
            manual_lights.push_back(new_light);
        }
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
    total_materials += debug_cube.materials.size();
    if (use_rt_box) {
        total_materials += rt_box.materials.size();
    }

    uint32_t shadow_sets_count = MAX_PANEL_LIGHTS * MAX_FRAMES_IN_FLIGHT;

    // We now allocate per-material, not per-object
    uint32_t total_sets = total_materials * MAX_FRAMES_IN_FLIGHT;
    
    // This MUST match your descriptor set layout
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, total_sets + shadow_sets_count),
        // 8 samplers (Albedo, Normal, Met/Rough, AO, Emissive, Transmission, Clearcoat, Clearcoat Roughness) per set
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, total_sets * (8 + MAX_PANEL_LIGHTS)),
        // We need 1 set per frame for the PPLL buffers
        // 1 storage image, 2 storage buffers per set
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT * 2)
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = total_sets + MAX_FRAMES_IN_FLIGHT+ shadow_sets_count; // TO CHECK
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size()); 
    pool_info.pPoolSizes = pool_sizes.data();

    descriptor_pool = vk::raii::DescriptorPool(logical_device, pool_info);
}

void Engine::createDescriptorSets()
{
    for(auto& obj : scene_objs){
        obj.createMaterialDescriptorSets(*this);
    }

    debug_cube.createMaterialDescriptorSets(*this);
    if (use_rt_box) {
        rt_box.createMaterialDescriptorSets(*this);
    }

    // torus.createMaterialDescriptorSets(*this);


    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        
        // Clear any old sets from the previous frame/swapchain
        shadow_descriptor_sets[i].clear();
        shadow_descriptor_sets[i].reserve(MAX_PANEL_LIGHTS); // Optional: improves performance

        for (int j = 0; j < MAX_PANEL_LIGHTS; ++j) {
            // Allocate one set using the shadow pipeline's layout
            vk::DescriptorSetAllocateInfo alloc_info(*descriptor_pool, 1, &*shadow_pipeline.descriptor_set_layout);
            
            // Move the newly allocated set into the vector
            shadow_descriptor_sets[i].push_back(std::move(logical_device.allocateDescriptorSets(alloc_info).front()));
            
            // Bind the corresponding shadow UBO
            vk::DescriptorBufferInfo buffer_info(shadow_ubos[i][j].buffer, 0, sizeof(ShadowUBO));
            
            // Use the set we just added (at index j)
            vk::WriteDescriptorSet write(*shadow_descriptor_sets[i][j], 0, 0, vk::DescriptorType::eUniformBuffer, {}, buffer_info);
            logical_device.updateDescriptorSets(write, {});
        }
    }
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


void Engine::createOITResources(){
    int avg_fragments_per_sample = 8;

    oit_max_fragments = swapchain.extent.width * swapchain.extent.height * static_cast<int>(mssa_samples) *
                        avg_fragments_per_sample;

    // 2. Atomic Counter Buffer
    // Initialized to 0 before OIT pass
    createBuffer(vma_allocator, sizeof(uint32_t),
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, // Need to clear it
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 oit_atomic_counter_buffer);

    // 3. Fragment List Buffer
    // Struct: vec4(color) + float(depth) + uint(next)
    // std430 layout: vec4(16), float(4), uint(4). Total = 24.
    // Next struct starts at 24. This is fine.
    // Let's use 32 bytes for safety and alignment.
    size_t node_size = sizeof(glm::vec4) + sizeof(float) + sizeof(uint32_t) + 8; // (16 + 4 + 4 + 8 padding) = 32
    vk::DeviceSize fragment_buffer_size = oit_max_fragments * node_size; 
    
    createBuffer(vma_allocator, fragment_buffer_size,
                 vk::BufferUsageFlagBits::eStorageBuffer,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 oit_fragment_list_buffer);

    // 4. Start Offset Image (Head Pointer)
    // Initialized to 0xFFFFFFFF before OIT pass
    oit_start_offset_image = Image::createImage(
        swapchain.extent.width, swapchain.extent.height, 1, mssa_samples,
        vk::Format::eR32Uint, // 32-bit unsigned int per pixel
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst, // Storage image + clear
        vk::MemoryPropertyFlagBits::eDeviceLocal, *this);
    
    // Use the 3-arg version of createImageView from GeneralHeaders.h
    oit_start_offset_image.image_view = Image::createImageView(
        oit_start_offset_image, *this);
    
    // Transition image to eGeneral layout for storage access
    // We do this once here, and it will stay in eGeneral forever.
    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_transfer, &logical_device);
    transitionImage(cmd, oit_start_offset_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);
    endSingleTimeCommands(cmd, transfer_queue);
}

void Engine::createOITCompositePipeline(){
    // 1. Create Descriptor Set Layout
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        // Binding 0: Atomic Counter
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1,
                                        vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 1: Fragment List
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                        vk::ShaderStageFlagBits::eFragment, nullptr),
        // Binding 2: Start Offset Image (Head Pointers)
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageImage, 1,
                                        vk::ShaderStageFlagBits::eFragment, nullptr)  
    };
    // This layout is shared, so store it on the engine
    oit_composite_pipeline.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);

    // 2. Create Pipeline
    
    oit_composite_pipeline.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &oit_composite_pipeline,
        v_shader_oit_composite,
        f_shader_oit_composite,
        TransparencyMode::OIT_COMPOSITE,
        vk::CullModeFlagBits::eNone
    );
}

void Engine::createOITDescriptorSets() {
    // This function now creates the *shared* PPLL descriptor set
    // oit_composite_descriptor_sets.clear(); // <-- REMOVED
    oit_ppll_descriptor_sets.clear();

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *oit_composite_pipeline.descriptor_set_layout);
    vk::DescriptorSetAllocateInfo alloc_info(
        *descriptor_pool,
        static_cast<uint32_t>(layouts.size()),
        layouts.data()
    );
    oit_ppll_descriptor_sets = logical_device.allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo atomic_counter_info;
        atomic_counter_info.buffer = oit_atomic_counter_buffer.buffer;
        atomic_counter_info.offset = 0;
        atomic_counter_info.range = sizeof(uint32_t);

        vk::DescriptorBufferInfo fragment_list_info;
        fragment_list_info.buffer = oit_fragment_list_buffer.buffer;
        fragment_list_info.offset = 0;
        fragment_list_info.range = VK_WHOLE_SIZE; // Use the whole buffer
        
        vk::DescriptorImageInfo start_offset_info;
        start_offset_info.imageView = *oit_start_offset_image.image_view;
        start_offset_info.imageLayout = vk::ImageLayout::eGeneral; // Stays in general layout
        
        std::array<vk::WriteDescriptorSet, 3> writes = {};
        
        // Binding 0: Atomic Counter
        writes[0].dstSet = *oit_ppll_descriptor_sets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = vk::DescriptorType::eStorageBuffer;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &atomic_counter_info;

        // Binding 1: Fragment List
        writes[1].dstSet = *oit_ppll_descriptor_sets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &fragment_list_info;

        // Binding 2: Start Offset Image
        writes[2].dstSet = *oit_ppll_descriptor_sets[i];
        writes[2].dstBinding = 2;
        writes[2].descriptorType = vk::DescriptorType::eStorageImage;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &start_offset_info;
        
        logical_device.updateDescriptorSets(writes, {});
    }
}

void Engine::createShadowResources() {
    shadow_map_format = findDepthFormat(physical_device);

    for (int i = 0; i < MAX_PANEL_LIGHTS; ++i) {
        // --- 1. Create Shadow Map Image (Manual VMA) ---
        vk::ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.extent = vk::Extent3D{SHADOW_MAP_DIM, SHADOW_MAP_DIM, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 6; // <-- FIX: Must be 6
        imageInfo.format = shadow_map_format;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
        imageInfo.samples = vk::SampleCountFlagBits::e1; // <-- FIX: Must be 1
        imageInfo.sharingMode = vk::SharingMode::eExclusive;
        imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible; // <-- FIX: Must have this flag

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocInfo.requiredFlags = (VkMemoryPropertyFlags)vk::MemoryPropertyFlagBits::eDeviceLocal;

        // Manually create the image using VMA
        VkImage vkImage;
        VkImageCreateInfo vkImageInfo = (VkImageCreateInfo)imageInfo; // Cast to C-style struct
        vmaCreateImage(vma_allocator, &vkImageInfo, &allocInfo,
                       &vkImage, &shadow_maps[i].allocation, nullptr);

        shadow_maps[i].image = vkImage;
        shadow_maps[i].image_format = static_cast<VkFormat>(shadow_map_format);
        shadow_maps[i].image_extent = imageInfo.extent;
        shadow_maps[i].mip_levels = 1;

        // --- 2. Create Image View (as a Cube) ---
        vk::ImageViewCreateInfo viewInfo;
        viewInfo.image = shadow_maps[i].image;
        viewInfo.viewType = vk::ImageViewType::eCube; // This is correct
        viewInfo.format = shadow_map_format;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 6; // This is correct
        
        shadow_maps[i].image_view = vk::raii::ImageView(logical_device, viewInfo);
    }

    // --- 3. Create Sampler (only one is needed) ---
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite; // Depth 1.0 (far)
    samplerInfo.compareEnable = vk::True;
    samplerInfo.compareOp = vk::CompareOp::eLess;
    samplerInfo.maxLod = 1.0f;
    
    shadow_sampler = vk::raii::Sampler(logical_device, samplerInfo);
}

void Engine::createShadowPipeline() {
    // 1. Descriptor Set Layout (for one ShadowUBO)
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, 
                                       vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
    };
    shadow_pipeline.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    
    // 2. Push Constant (Model Matrix)
    vk::PushConstantRange push_constant_range(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    
    // 3. Pipeline Layout
    vk::PipelineLayoutCreateInfo pipeline_layout_info({}, 1, &*shadow_pipeline.descriptor_set_layout, 1, &push_constant_range);
    shadow_pipeline.layout = vk::raii::PipelineLayout(logical_device, pipeline_layout_info);
    
    // 4. Create Pipeline (using the new shadow pipeline function)
    shadow_pipeline.pipeline = Pipeline::createShadowPipeline(
        *this,
        &shadow_pipeline,
        v_shader_shadow,
        f_shader_shadow
    );
}


// ------ Render Loop Functions

void Engine::run(){
    {
        vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_transfer, &logical_device);

        for (int i = 0; i < MAX_PANEL_LIGHTS; ++i) {
            transitionImage(cmd, shadow_maps[i].image,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::ImageAspectFlagBits::eDepth);
        }

        endSingleTimeCommands(cmd, transfer_queue);
    }

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        drawFrame();
    }

    logical_device.waitIdle();

    cleanup();
}

void Engine::recordCommandBuffer(uint32_t image_index){
    auto& cmd = graphics_command_buffer[current_frame];
    cmd.begin({});
    if (panel_shadows_enabled) {
        
        int panel_index = 0;
        // IMPORTANT: This loop MUST match the logic in updateUniformBuffer()
        for (int i = 0; i < panel_lights.size(); ++i) {
            // Skip if this light is off
            if (!panel_lights_on[i]) {
                continue; // Don't increment shadow_map_slot here!
            }

            // Transition shadow map to be a depth attachment
            transitionImage(cmd, shadow_maps[panel_index].image, 
                            vk::ImageLayout::eShaderReadOnlyOptimal, 
                            vk::ImageLayout::eDepthStencilAttachmentOptimal, 
                            vk::ImageAspectFlagBits::eDepth);

            vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);
            vk::RenderingAttachmentInfo depth_attachment_info;
            depth_attachment_info.imageView = *shadow_maps[panel_index].image_view;
            depth_attachment_info.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
            depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
            depth_attachment_info.clearValue = clear_depth;

            vk::RenderingInfo shadow_rendering_info;
            shadow_rendering_info.renderArea.offset = vk::Offset2D{0, 0};
            shadow_rendering_info.renderArea.extent = vk::Extent2D{SHADOW_MAP_DIM, SHADOW_MAP_DIM};
            shadow_rendering_info.layerCount = 6;
            shadow_rendering_info.viewMask = 0b00111111;
            shadow_rendering_info.colorAttachmentCount = 0;
            shadow_rendering_info.pDepthAttachment = &depth_attachment_info;

            cmd.beginRendering(shadow_rendering_info);

            cmd.setViewport(0, vk::Viewport(0.f, 0.f, (float)SHADOW_MAP_DIM, (float)SHADOW_MAP_DIM, 0.f, 1.f));
            cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), {SHADOW_MAP_DIM, SHADOW_MAP_DIM}));
            cmd.setCullMode(vk::CullModeFlagBits::eFront);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadow_pipeline.pipeline);
            
            // Bind the correct Shadow UBO for this pass
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *shadow_pipeline.layout, 0, 
                     {*shadow_descriptor_sets[current_frame][panel_index]}, {});

            // --- Draw all shadow-casting objects ---
            for(auto& obj : scene_objs) {
                if (obj.o_primitives.empty()) continue; 
                
                cmd.bindVertexBuffers(0, {obj.geometry_buffer.buffer}, {0});
                cmd.bindIndexBuffer(obj.geometry_buffer.buffer, obj.index_buffer_offset, vk::IndexType::eUint32);
                cmd.pushConstants<glm::mat4>(*shadow_pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, obj.model_matrix);
                
                for(auto& primitive : obj.o_primitives) {
                    cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, 0, 0);
                }
            }
            
            // Draw rt_box
            if (use_rt_box && rt_box.o_pipeline && !rt_box.o_primitives.empty()) {
                cmd.bindVertexBuffers(0, {rt_box.geometry_buffer.buffer}, {0});
                cmd.bindIndexBuffer(rt_box.geometry_buffer.buffer, rt_box.index_buffer_offset, vk::IndexType::eUint32);
                cmd.pushConstants<glm::mat4>(*shadow_pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, rt_box.model_matrix);
                for(auto& primitive : rt_box.o_primitives) {
                    cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, 0, 0);
                }
            }

            cmd.endRendering();

            // Transition shadow map to be readable in PBR shaders
            transitionImage(cmd, shadow_maps[panel_index].image, 
                            vk::ImageLayout::eDepthStencilAttachmentOptimal, 
                            vk::ImageLayout::eShaderReadOnlyOptimal, 
                            vk::ImageAspectFlagBits::eDepth);

            panel_index++;
        }
    }
    // --- 1. GATHER TRANSPARENTS ---
    transparent_draws.clear();
    for(auto& obj : scene_objs){
        if (!obj.t_primitives.empty()) {
            for(const auto& primitive : obj.t_primitives) {
                const Material& material = obj.materials[primitive.material_index];
                transparent_draws.push_back({&obj, &primitive, &material, 0.0f}); // Distance doesn't matter
            }
        }
    }

    // --- 2. OPAQUE PASS ---
    {
        // Transition swapchain image (for final resolve)
        transition_image_layout(
            image_index,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {}, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clear_color = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f);
        vk::ClearValue clear_depth = vk::ClearDepthStencilValue(1.0f, 0);

        // Main color attachment
        vk::RenderingAttachmentInfo color_attachment_info{};
        color_attachment_info.imageView = color_image.image_view;
        color_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        color_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
        color_attachment_info.storeOp = vk::AttachmentStoreOp::eStore; // MUST store for composite pass
        color_attachment_info.clearValue = clear_color;

        // Main depth attachment
        vk::RenderingAttachmentInfo depth_attachment_info = {};
        depth_attachment_info.imageView = depth_image.image_view;
        depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
        depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore; // MUST store for OIT pass
        depth_attachment_info.clearValue = clear_depth;

        vk::RenderingInfo rendering_info;
        rendering_info.renderArea.offset = vk::Offset2D{0, 0};
        rendering_info.renderArea.extent = swapchain.extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment_info;
        rendering_info.pDepthAttachment = &depth_attachment_info;

        cmd.beginRendering(rendering_info);

        cmd.setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.f, 1.f));
        cmd.setScissor(0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapchain.extent));

        // --- OPAQUE DRAW LOOP ---
        PipelineInfo* last_bound_pipeline = nullptr;
        Gameobject* last_bound_object = nullptr;

        for(auto& obj : scene_objs){
            if (obj.o_primitives.empty()) continue; // Skip if no opaque parts

            // Bind pipeline if different
            if (obj.o_pipeline != last_bound_pipeline) {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *obj.o_pipeline->pipeline);
                last_bound_pipeline = obj.o_pipeline;
            }

            // Bind geometry if different
            if (&obj != last_bound_object) {
                cmd.bindVertexBuffers(0, {obj.geometry_buffer.buffer}, {0});
                cmd.bindIndexBuffer(obj.geometry_buffer.buffer, obj.index_buffer_offset, vk::IndexType::eUint32);
                last_bound_object = &obj;
            }
            
            // Push vertex constant (model matrix)
            cmd.pushConstants<glm::mat4>(*obj.o_pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, obj.model_matrix);

            // Loop and draw all opaque primitives for this object
            for(auto& primitive : obj.o_primitives) {
                const Material& material = obj.materials[primitive.material_index];

                if(material.is_doublesided){
                    cmd.setCullMode(vk::CullModeFlagBits::eNone);
                }else{
                    cmd.setCullMode(vk::CullModeFlagBits::eBack);
                }

                // Setup material push constant
                MaterialPushConstant p_const;
                p_const.base_color_factor = material.base_color_factor;
                p_const.emissive_factor_and_pad = glm::vec4(material.emissive_factor, 0.0f);
                p_const.metallic_factor = material.metallic_factor;
                p_const.roughness_factor = material.roughness_factor;
                p_const.occlusion_strength = material.occlusion_strength;
                p_const.specular_factor = material.specular_factor;
                p_const.specular_color_factor = material.specular_color_factor;
                p_const.transmission_factor = material.transmission_factor;
                p_const.alpha_cutoff = material.alpha_cutoff;
                p_const.clearcoat_factor = material.clearcoat_factor;
                p_const.clearcoat_roughness_factor = material.clearcoat_roughness_factor;

                cmd.pushConstants<MaterialPushConstant>(*obj.o_pipeline->layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), p_const);

                // Bind material descriptor set
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *obj.o_pipeline->layout, 0, {*material.descriptor_sets[current_frame]}, {});
                
                // Draw
                cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, 0, 0);
            }
        }

        if (use_rt_box && rt_box.o_pipeline && !rt_box.o_primitives.empty())
        {
            // Bind pipeline ONCE
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *rt_box.o_pipeline->pipeline);
            
            // Bind geometry ONCE
            cmd.bindVertexBuffers(0, {rt_box.geometry_buffer.buffer}, {0});
            cmd.bindIndexBuffer(rt_box.geometry_buffer.buffer, rt_box.index_buffer_offset, vk::IndexType::eUint32);
            
            // Push vertex constant (model matrix) ONCE
            cmd.pushConstants<glm::mat4>(*rt_box.o_pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, rt_box.model_matrix);
            
            // The RT box is not double-sided, so we set cull mode to back-face
            // This is correct because our vertices are wound for inward-facing normals
            cmd.setCullMode(vk::CullModeFlagBits::eBack);

            // Loop through the 5 primitives (floor, ceil, back, left, right)
            for(auto& primitive : rt_box.o_primitives)
            {
                const Material& material = rt_box.materials[primitive.material_index];

                // Bind material descriptor set
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *rt_box.o_pipeline->layout, 0, {*material.descriptor_sets[current_frame]}, {});

                // Setup material push constant
                MaterialPushConstant p_const;
                p_const.base_color_factor = material.base_color_factor;
                p_const.metallic_factor = material.metallic_factor;
                p_const.roughness_factor = material.roughness_factor;
                p_const.occlusion_strength = material.occlusion_strength;
                p_const.specular_factor = material.specular_factor;
                p_const.specular_color_factor = material.specular_color_factor;
                p_const.transmission_factor = material.transmission_factor;
                p_const.alpha_cutoff = material.alpha_cutoff;
                p_const.clearcoat_factor = material.clearcoat_factor;
                p_const.clearcoat_roughness_factor = material.clearcoat_roughness_factor;
                p_const.emissive_factor_and_pad = glm::vec4(material.emissive_factor, 0.f);

                cmd.pushConstants<MaterialPushConstant>(*rt_box.o_pipeline->layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), p_const);
                
                // Draw
                cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, 0, 0);
            }
        }

        if (debug_lights && debug_cube.o_pipeline && !debug_cube.o_primitives.empty())
        {
            // Bind pipeline ONCE
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *debug_cube.o_pipeline->pipeline);
            
            // Bind geometry ONCE
            cmd.bindVertexBuffers(0, {debug_cube.geometry_buffer.buffer}, {0});
            cmd.bindIndexBuffer(debug_cube.geometry_buffer.buffer, debug_cube.index_buffer_offset, vk::IndexType::eUint32);

            // Get material and bind descriptor set ONCE
            const Material& material = debug_cube.materials[0];
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *debug_cube.o_pipeline->layout, 0, {*material.descriptor_sets[current_frame]}, {});

            // Setup and push fragment push constant ONCE
            MaterialPushConstant p_const;
            p_const.base_color_factor = material.base_color_factor;
            p_const.emissive_factor_and_pad = glm::vec4(material.emissive_factor, 0.0f);
            p_const.metallic_factor = material.metallic_factor;
            p_const.roughness_factor = material.roughness_factor;
            p_const.occlusion_strength = 1.0f; // Use default
            p_const.specular_factor = 0.5f; // Use default
            p_const.specular_color_factor = glm::vec3(1.f); // Use default
            p_const.transmission_factor = 0.0f;
            p_const.alpha_cutoff = 0.0f;
            p_const.clearcoat_factor = 0.0f;
            p_const.clearcoat_roughness_factor = 0.0f;
            
            cmd.pushConstants<MaterialPushConstant>(*debug_cube.o_pipeline->layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), p_const);

            // Loop through lights
            for(int i = 0; i < ubo.curr_num_pointlights; i++)
            {
                // Create model matrix for this light
                // Translate to its position and scale it down
                glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(ubo.pointlights[i].position));
                model_matrix = glm::scale(model_matrix, glm::vec3(0.5f)); // 0.5m cubes

                // Push vertex constant (model matrix)
                cmd.pushConstants<glm::mat4>(*debug_cube.o_pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, model_matrix);

                // Draw
                cmd.drawIndexed(debug_cube.o_primitives[0].index_count, 1, debug_cube.o_primitives[0].first_index, 0, 0);
            }
        }

        cmd.endRendering();
    }

    // --- 3. OIT WRITE PASS ---
    // Renders to PPLL buffers (SSBOs)
    // Reads from msaa depth_image
    {
        // --- CLEAR OIT BUFFERS ---
        // We must clear the atomic counter to 0
        cmd.fillBuffer(oit_atomic_counter_buffer.buffer, 0, sizeof(uint32_t), 0);
        
        // We must clear the head-pointer image to 0xFFFFFFFF (our "null" pointer)
        vk::ClearColorValue clear_uint(std::array<uint32_t, 4>{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF });
        vk::ImageSubresourceRange clear_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        cmd.clearColorImage(oit_start_offset_image.image, 
                            vk::ImageLayout::eGeneral, // Image is already in eGeneral
                            clear_uint, clear_range);

        // Add a barrier to ensure clears are finished before shader writes
        vk::MemoryBarrier2 mem_barrier;
        mem_barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        mem_barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        mem_barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        mem_barrier.dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderRead;
        vk::DependencyInfo dep_info;
        dep_info.memoryBarrierCount = 1;
        dep_info.pMemoryBarriers = &mem_barrier;
        cmd.pipelineBarrier2(dep_info);
        // --- END OIT CLEAR ---

        // Depth attachment (LOAD, don't clear, don't write)
        vk::RenderingAttachmentInfo depth_attachment_info = {};
        depth_attachment_info.imageView = depth_image.image_view;
        depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        depth_attachment_info.loadOp = vk::AttachmentLoadOp::eLoad; // LOAD opaque depth
        depth_attachment_info.storeOp = vk::AttachmentStoreOp::eDontCare; // Don't need it after this
        
        vk::RenderingInfo oit_rendering_info;
        oit_rendering_info.renderArea.offset = vk::Offset2D{0, 0};
        oit_rendering_info.renderArea.extent = swapchain.extent;
        oit_rendering_info.layerCount = 1;
        oit_rendering_info.colorAttachmentCount = 0; // No color attachments
        oit_rendering_info.pColorAttachments = nullptr;
        oit_rendering_info.pDepthAttachment = &depth_attachment_info;

        cmd.beginRendering(oit_rendering_info);
        
        // Set viewport/scissor again
        cmd.setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.f, 1.f));
        cmd.setScissor(0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapchain.extent));


        // --- OIT WRITE DRAW LOOP ---
        if (!transparent_draws.empty()) {
            Gameobject* last_bound_object = nullptr;
            PipelineInfo* last_bound_pipeline = nullptr;

            for (const auto& draw : transparent_draws) {
                Gameobject* obj = draw.object;
                const Primitive& primitive = *draw.primitive;
                const Material& material = *draw.material;
                PipelineInfo* pipeline = obj->t_pipeline; // Get OIT_WRITE pipeline

                if(material.is_doublesided){
                    cmd.setCullMode(vk::CullModeFlagBits::eNone);
                }else{
                    cmd.setCullMode(vk::CullModeFlagBits::eBack);
                }

                // Bind pipeline if different
                if (pipeline != last_bound_pipeline) {
                    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline);
                    last_bound_pipeline = pipeline;
                }
                
                // Bind geometry if different
                if (obj != last_bound_object) {
                    cmd.bindVertexBuffers(0, {obj->geometry_buffer.buffer}, {0});
                    cmd.bindIndexBuffer(obj->geometry_buffer.buffer, obj->index_buffer_offset, vk::IndexType::eUint32);
                    last_bound_object = obj;
                }

                // Push vertex constant (model matrix)
                cmd.pushConstants<glm::mat4>(*pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, obj->model_matrix);

                // Setup material push constant
                MaterialPushConstant p_const;
                p_const.base_color_factor = material.base_color_factor;
                p_const.emissive_factor_and_pad = glm::vec4(material.emissive_factor, 0.0f);
                p_const.metallic_factor = material.metallic_factor;
                p_const.roughness_factor = material.roughness_factor;
                p_const.occlusion_strength = material.occlusion_strength;
                p_const.specular_factor = material.specular_factor;
                p_const.specular_color_factor = material.specular_color_factor;
                p_const.transmission_factor = material.transmission_factor;
                p_const.alpha_cutoff = material.alpha_cutoff;
                p_const.clearcoat_factor = material.clearcoat_factor;
                p_const.clearcoat_roughness_factor = material.clearcoat_roughness_factor;


                cmd.pushConstants<MaterialPushConstant>(*pipeline->layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), p_const);

                // Bind *both* descriptor sets
                std::array<vk::DescriptorSet, 2> descriptor_sets_to_bind = {
                    *material.descriptor_sets[current_frame], // Set 0: Material
                    *oit_ppll_descriptor_sets[current_frame]  // Set 1: PPLL Buffers
                };

                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     *pipeline->layout, 0, // Start at set 0
                                     descriptor_sets_to_bind, // Bind 2 sets
                                     {});
                
                // Draw
                cmd.drawIndexed(primitive.index_count, 1, primitive.first_index, 0, 0);
            }
        }
        cmd.endRendering();
    }

    // --- 4. RESOLVE PASS ---
    // We add a barrier to ensure all SSBO writes from the OIT Write pass
    // are finished before the Composite pass reads from them.
    {
        vk::MemoryBarrier2 mem_barrier;
        mem_barrier.srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        mem_barrier.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
        mem_barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        mem_barrier.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead;
        vk::DependencyInfo dep_info;
        dep_info.memoryBarrierCount = 1;
        dep_info.pMemoryBarriers = &mem_barrier;
        cmd.pipelineBarrier2(dep_info);
    }

    // --- 5. COMPOSITE PASS ---
    // Renders fullscreen quad to msaa color_image
    // Reads from PPLL SSBOs
    // Blends OVER the existing opaque scene in color_image
    // Resolves final msaa color_image to swapchain image
    {
        // Main color attachment (LOAD, don't clear)
        vk::RenderingAttachmentInfo color_attachment_info{};
        color_attachment_info.imageView = color_image.image_view;
        color_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        color_attachment_info.loadOp = vk::AttachmentLoadOp::eLoad; // LOAD opaque scene
        color_attachment_info.storeOp = vk::AttachmentStoreOp::eDontCare; // Don't care, we resolve
        
        // Add the resolve op here
        color_attachment_info.resolveMode = vk::ResolveModeFlagBits::eAverage;
        color_attachment_info.resolveImageView = swapchain.image_views[image_index];
        color_attachment_info.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::RenderingInfo composite_rendering_info;
        composite_rendering_info.renderArea.offset = vk::Offset2D{0, 0};
        composite_rendering_info.renderArea.extent = swapchain.extent;
        composite_rendering_info.layerCount = 1;
        composite_rendering_info.colorAttachmentCount = 1;
        composite_rendering_info.pColorAttachments = &color_attachment_info;
        composite_rendering_info.pDepthAttachment = nullptr; // No depth test/write

        cmd.beginRendering(composite_rendering_info);
        
        cmd.setViewport(0, vk::Viewport(0.f, 0.f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.f, 1.f));
        cmd.setScissor(0, vk::Rect2D( vk::Offset2D( 0, 0 ), swapchain.extent));
        cmd.setCullMode(vk::CullModeFlagBits::eNone);

        // Bind composite pipeline and descriptors
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *oit_composite_pipeline.pipeline);
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *oit_composite_pipeline.layout, 0, // Bind to Set 0
            *oit_ppll_descriptor_sets[current_frame], // The PPLL descriptor set
            nullptr
        );
        
        // Draw fullscreen triangle (3 vertices)
        cmd.draw(3, 1, 0, 0);

        cmd.endRendering();
    }


    // --- 6. FINAL TRANSITION ---
    // Transition the swapchain image for presentation
    transition_image_layout(
        image_index,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );

    cmd.end();
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
    // --- 1. Main UBO ---
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.camera_pos = camera.getCurrentState().f_camera.position;

    // --- RESET EVERYTHING ---
    ubo.curr_num_pointlights = 0;
    ubo.curr_num_shadowlights = 0;
    ubo.panel_shadows_enabled = this->panel_shadows_enabled;
    ubo.shadow_far_plane = this->shadow_light_far_plane;

    // --- 1. Add Panel Lights (if enabled) ---
    for(int i = 0; i < panel_lights.size(); ++i) {
        if (ubo.curr_num_shadowlights >= MAX_POINTLIGHTS) break;
        // Only add active lights
        if (panel_lights_on[i]) {
            int ubo_index = ubo.curr_num_shadowlights;
            ubo.shadowlights[ubo_index] = panel_lights[i];
            
            ubo.curr_num_shadowlights++;
        }
    }

    // --- 2. Add Manual Lights ---
    if (use_manual_lights) {
        for (const auto& light : manual_lights) {
            if (ubo.curr_num_pointlights >= MAX_POINTLIGHTS) break;
            ubo.pointlights[ubo.curr_num_pointlights++] = light;
            // These lights have no shadows, so light_to_shadow_map_index stays -1
        }
    }

    // --- 3. Add Emissive Lights ---
    if (use_emissive_lights) {
        for (const auto& light : emissive_lights) {
            if (ubo.curr_num_pointlights >= MAX_POINTLIGHTS) break;
            ubo.pointlights[ubo.curr_num_pointlights++] = light;
            // These lights have no shadows, so light_to_shadow_map_index stays -1
        }
    }

    // --- 4. NOW Setup Shadow Maps for Panel Lights ---
    // We do this AFTER adding all lights so indices are stable
    int panel_index = 0;
    if (panel_shadows_enabled) {
        for(int i = 0; i < panel_lights.size(); ++i) {
            // Skip if this light is off or if we've run out of shadow maps
            if (!panel_lights_on[i]) {
                continue;
            }
            
            // --- Update the corresponding Shadow UBO ---
            Pointlight& light = panel_lights[i];
            shadow_ubo_data.lightPos = light.position;
            shadow_ubo_data.farPlane = shadow_light_far_plane;
            
            shadow_ubo_data.proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, shadow_light_far_plane);
            glm::vec3 pos = glm::vec3(light.position);
            shadow_ubo_data.views[0] = glm::lookAt(pos, pos + glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            shadow_ubo_data.views[1] = glm::lookAt(pos, pos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            shadow_ubo_data.views[2] = glm::lookAt(pos, pos + glm::vec3( 0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
            shadow_ubo_data.views[3] = glm::lookAt(pos, pos + glm::vec3( 0.0,-1.0, 0.0), glm::vec3(0.0, 0.0,-1.0));
            shadow_ubo_data.views[4] = glm::lookAt(pos, pos + glm::vec3( 0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
            shadow_ubo_data.views[5] = glm::lookAt(pos, pos + glm::vec3( 0.0, 0.0,-1.0), glm::vec3(0.0, -1.0, 0.0));
            
            memcpy(shadow_ubos_mapped[current_image][panel_index], &shadow_ubo_data, sizeof(ShadowUBO));
            panel_index++;
        }
    }

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

    vmaDestroyImage(vma_allocator, color_image.image, color_image.allocation);
    color_image = Image::createImage(swapchain.extent.width, swapchain.extent.height,
                        1, mssa_samples, swapchain.format, vk::ImageTiling::eOptimal, 
                    vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, *this);
    color_image.image_view = Image::createImageView(color_image, *this);


    vmaDestroyBuffer(vma_allocator, oit_atomic_counter_buffer.buffer, oit_atomic_counter_buffer.allocation);
    vmaDestroyBuffer(vma_allocator, oit_fragment_list_buffer.buffer, oit_fragment_list_buffer.allocation);
    vmaDestroyImage(vma_allocator, oit_start_offset_image.image, oit_start_offset_image.allocation);
    createOITResources();
    createOITDescriptorSets();

    camera.modAspectRatio(swapchain.extent.width * 1.0 / swapchain.extent.height);

    std::cout << "Memory After Swapchain Recreation" << std::endl;
    printGpuMemoryUsage();

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

