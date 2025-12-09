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
void Engine::readBuffer(vk::Buffer buffer, vk::DeviceSize size, void* dst_ptr) {
    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, size, vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 staging_buffer);

    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_graphics, &logical_device);
    vk::BufferCopy copy_region(0, 0, size);
    cmd.copyBuffer(buffer, staging_buffer.buffer, copy_region);
    endSingleTimeCommands(cmd, graphics_queue);

    void* data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(dst_ptr, data, (size_t)size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);
}

// 2. The main Importance Sampling Logic
void Engine::updateImportanceSampling() {
    std::cout << "Calculating Importance Samples..." << std::endl;

    // A. Read back Hit Data
    size_t num_hits = sampling_points.size();
    vk::DeviceSize buffer_size = sizeof(HitDataGPU) * num_hits;
    std::vector<HitDataGPU> raw_hits(num_hits);
    readBuffer(hit_data_buffer.buffer, buffer_size, raw_hits.data());

    SamplingMethod method = sampling_methods[current_sampling];

    if (method == SamplingMethod::IMP_COL) {
        // Extract Colors
        std::vector<glm::vec4> prev_colors;
        prev_colors.reserve(num_hits);
        for(const auto& hit : raw_hits) {
            prev_colors.push_back(glm::vec4(hit.r, hit.g, hit.b, hit.a));
        }
        // Generate based on Color Gradients
        Sampling::generateImportanceSamples(sampling_points, num_rays, sampling_points, prev_colors);
    } 
    else if (method == SamplingMethod::IMP_HIT) {
        // Extract Flags
        std::vector<float> prev_flags;
        prev_flags.reserve(num_hits);
        for(const auto& hit : raw_hits) {
            prev_flags.push_back(hit.flag);
        }
        // Generate based on Hit/Miss
        Sampling::generateHitBasedImportanceSamples(sampling_points, num_rays, sampling_points, prev_flags);
    }

    // D. Upload NEW samples to GPU
    vk::DeviceSize sample_size = sizeof(RaySample) * sampling_points.size();
    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, sample_size, vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, staging_buffer);
    
    void* data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, sampling_points.data(), (size_t)sample_size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    copyBuffer(staging_buffer.buffer, sample_data_buffer.buffer, sample_size,
               command_pool_graphics, &logical_device, graphics_queue);
               
    // Staging buffer is destroyed automatically
}

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
            mat_config["base_color"][0].get<float>(),
            mat_config["base_color"][1].get<float>(),
            mat_config["base_color"][2].get<float>(),
            1.0f
        );
        mat.metallic_factor = mat_config.value("metallic", 0.0f);
        mat.roughness_factor = mat_config.value("roughness", 1.0f);
        mat.emissive_factor = glm::vec3(mat_config["base_color"][0].get<float>(),
            mat_config["base_color"][1].get<float>(),
            mat_config["base_color"][2].get<float>()); // All materials are non-emissive
        mat.occlusion_strength = 1.0f;
        mat.albedo_texture_index = 0;
        mat.is_transparent = false;
        return mat;
    };

    // --- 4. Create Materials, Primitives, and Lights ---
    
    // Clear old data
    rt_box.materials.clear();
    rt_box.o_primitives.clear();

    std::vector<std::string> panel_names = {"floor", "ceiling", "back_wall", "left_wall", "right_wall"};

    for (int i = 0; i < 5; ++i) {
        const std::string& name = panel_names[i];
        const json& panel_config = config["panels"][name];

        // Create material
        Material mat = createMatFromJson(panel_config["material"]);
        mat.emissive_factor *= panel_config["light"].value("intensity", 0.0f);
        rt_box.materials.push_back(mat);
        
        // Create primitive (indices are 6 per face, starting at 0, 6, 12, etc.)
        rt_box.o_primitives.push_back({
            static_cast<uint32_t>(i * 6), // first_index
            6,                            // index_count
            i                             // material_index
        });
    }

    rt_box.emissive_triangles.clear();
    for(size_t k = 0; k < rt_box.indices.size(); k += 3){
        uint32_t index = static_cast<int>(k / 6);
        if(glm::length(rt_box.materials[index].emissive_factor) < 0.00001f){
            continue;
        }

        uint32_t idx0 = rt_box.indices[k];
        uint32_t idx1 = rt_box.indices[k+1];
        uint32_t idx2 = rt_box.indices[k+2];

        const auto& p0 = rt_box.vertices[idx0].pos;
        const auto& p1 = rt_box.vertices[idx1].pos;
        const auto& p2 = rt_box.vertices[idx2].pos;

        EmissiveTriangle tri;
        tri.index0 = idx0;
        tri.index1 = idx1;
        tri.index2 = idx2;
        tri.material_index = index;
        tri.area = 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
        rt_box.emissive_triangles.push_back(tri);
    }

    // --- 5. GPU Resources ---
    rt_box.textures.emplace_back();
    rt_box.default_sampler = Image::createTextureSampler(physical_device, &logical_device, 1);
    rt_box.createDefaultTexture(*this, rt_box.textures[0], glm::vec4(125, 125, 125, 255));
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
    createBuffer(vma_allocator, total_size, 
        vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);

    void * data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, obj.vertices.data(), (size_t)vertex_size);
    memcpy((char *)data + vertex_size, obj.indices.data(), (size_t)index_size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    createBuffer(vma_allocator, total_size, 
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer |vk::BufferUsageFlagBits::eIndexBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                vk::MemoryPropertyFlagBits::eDeviceLocal, obj.geometry_buffer);
    
    copyBuffer(staging_buffer.buffer, obj.geometry_buffer.buffer, total_size,
                command_pool_transfer, &logical_device, transfer_queue);

    // We need to exclude the torus, since it casts the rays
    // For the moment
    if (&obj != &torus && !obj.o_primitives.empty()) {
        buildBlas(obj);
    }
}

/**
 * @brief Helper function to get the 64-bit device address of a buffer.
 * Requires the eShaderDeviceAddress usage flag to be set on the buffer.
 */
uint64_t Engine::getBufferDeviceAddress(vk::Buffer buffer) {
    vk::BufferDeviceAddressInfo info(buffer);
    return logical_device.getBufferAddress(info);
}

/**
 * @brief Builds a Bottom-Level Acceleration Structure (BLAS) for a single Gameobject.
 * This function iterates over all OPAQUE primitives in the object.
 */
void Engine::buildBlas(Gameobject& obj)
{
    // 1. Collect geometry info for all opaque primitives
    std::vector<vk::AccelerationStructureGeometryKHR> geometries;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> build_ranges;
    std::vector<uint32_t> primitive_counts; // Number of triangles per geometry

    uint64_t vertex_buffer_addr = getBufferDeviceAddress(obj.geometry_buffer.buffer);
    uint64_t index_buffer_addr = vertex_buffer_addr + obj.index_buffer_offset;

    for (const auto& prim : obj.o_primitives) {
        if (prim.index_count < 3) {
            continue;
        }
        uint32_t num_triangles = prim.index_count / 3;
        
        vk::AccelerationStructureGeometryTrianglesDataKHR triangles_data;
        triangles_data.vertexFormat = vk::Format::eR32G32B32Sfloat;
        triangles_data.vertexData.deviceAddress = vertex_buffer_addr;
        triangles_data.vertexStride = sizeof(Vertex);
        triangles_data.maxVertex = obj.vertices.size() - 1;
        triangles_data.indexType = vk::IndexType::eUint32;
        // Point to the *start* of this primitive's indices
        triangles_data.indexData.deviceAddress = index_buffer_addr + (prim.first_index * sizeof(uint32_t));

        vk::AccelerationStructureGeometryKHR geo;
        geo.geometryType = vk::GeometryTypeKHR::eTriangles;
        geo.geometry.triangles = triangles_data;
        if (obj.materials[prim.material_index].is_transparent) {
            geo.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation; 
            // eNoDuplicate... helps performance by ensuring AnyHit runs only once per potential hit
        } else {
            geo.flags = vk::GeometryFlagBitsKHR::eOpaque; // Keep Opaque for solid objects (Performance optimization)
        }

        geometries.push_back(geo);

        // Define the build range for this primitive
        vk::AccelerationStructureBuildRangeInfoKHR range_info;
        range_info.primitiveCount = num_triangles;
        range_info.primitiveOffset = 0; // Offset *within* the indexData range
        range_info.firstVertex = 0;
        range_info.transformOffset = 0;
        
        build_ranges.push_back(range_info);
        primitive_counts.push_back(num_triangles);
    }

    // 2. Get Build Sizes
    if (geometries.empty()) {
        return;
    }
    vk::AccelerationStructureBuildGeometryInfoKHR build_info;
    build_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    build_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    build_info.geometryCount = static_cast<uint32_t>(geometries.size());
    build_info.pGeometries = geometries.data();

    vk::AccelerationStructureBuildSizesInfoKHR size_info;
    vkGetAccelerationStructureBuildSizesKHR(
        *logical_device,
        static_cast<VkAccelerationStructureBuildTypeKHR>(vk::AccelerationStructureBuildTypeKHR::eDevice),
        reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(&build_info),
        primitive_counts.data(),
        reinterpret_cast<VkAccelerationStructureBuildSizesInfoKHR*>(&size_info)
    );

    // 3. Create AS Buffer
    createBuffer(vma_allocator, size_info.accelerationStructureSize,
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 obj.blas.buffer);

    // 4. Create AS Object
    vk::AccelerationStructureCreateInfoKHR as_create_info;
    as_create_info.buffer = obj.blas.buffer.buffer;
    as_create_info.size = size_info.accelerationStructureSize;
    as_create_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

    VkAccelerationStructureKHR vk_as;
    if (vkCreateAccelerationStructureKHR(*logical_device, reinterpret_cast<const VkAccelerationStructureCreateInfoKHR*>(&as_create_info), nullptr, &vk_as) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create acceleration structure!");
    }
    obj.blas.as = vk::raii::AccelerationStructureKHR(logical_device, vk_as);

    // 5. Create Scratch Buffer
    AllocatedBuffer scratch_buffer;
    createBuffer(vma_allocator, size_info.buildScratchSize,
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 scratch_buffer);
    uint64_t scratch_addr = getBufferDeviceAddress(scratch_buffer.buffer);

    // 6. Build the BLAS on the GPU
    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_graphics, &logical_device);

    build_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    build_info.dstAccelerationStructure = *obj.blas.as;
    build_info.scratchData.deviceAddress = scratch_addr;

    // We pass an array of pointers to the build range infos
    const VkAccelerationStructureBuildRangeInfoKHR* p_build_ranges_c = 
        reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR*>(build_ranges.data());
    
    const VkAccelerationStructureBuildRangeInfoKHR * const p_build_ranges_const_ptr = p_build_ranges_c;
    vkCmdBuildAccelerationStructuresKHR(*cmd, 1, reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(&build_info), 
                &p_build_ranges_const_ptr);

    // 7. Add a barrier to wait for the build to finish
    vk::MemoryBarrier2 barrier;
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    barrier.srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    barrier.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR;
    cmd.pipelineBarrier2(vk::DependencyInfo({}, 1, &barrier, 0, nullptr, 0, nullptr));

    endSingleTimeCommands(cmd, graphics_queue);

    // 8. Get the device address for the TLAS
    vk::AccelerationStructureDeviceAddressInfoKHR addr_info(*obj.blas.as);
    obj.blas.device_address = logical_device.getAccelerationStructureAddressKHR(addr_info);
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

        case Action::FOV_UP:  input.fov_up     = pressed; engine -> accumulation_frame = 0; break;
        case Action::FOV_DOWN:  input.fov_down   = pressed; engine -> accumulation_frame = 0; break;
        case Action::HEIGHT_UP: input.height_up = pressed; break;
        case Action::HEIGHT_DOWN: input.height_down = pressed; break;
        case Action::RESET: input.reset = pressed; break;
        case Action::SWITCH: input.change = pressed; engine -> accumulation_frame = 0; break;

        case Action::MAJ_RAD_UP: input.maj_rad_up = pressed; break;
        case Action::MAJ_RAD_DOWN: input.maj_rad_down = pressed; break;
        case Action::MIN_RAD_UP: input.min_rad_up = pressed; break;
        case Action::MIN_RAD_DOWN: input.min_rad_down = pressed; break;

        case Action::POINTCLOUD:
            if (action == GLFW_PRESS){
                engine->render_point_cloud = !engine->render_point_cloud;
                engine -> accumulation_frame = 0;
            }
            break;
        case Action::F_POINTCLOUD:
            if (action == GLFW_PRESS)
                engine->render_final_pointcloud = !engine->render_final_pointcloud;
            break;

        case Action::TOGGLE_PROJECTION:
            if (action == GLFW_PRESS)
                engine->show_projected_torus = !engine->show_projected_torus;
            break;
        
        case Action::CAPTURE_DATA: 
            if (action == GLFW_PRESS){
                engine->is_capturing = true;
                engine -> image_captured_count = 0;
            }
            break;
        
        case Action::SAMPLING_METHOD: 
            if (action == GLFW_PRESS){
                engine -> accumulation_frame = 0;
                engine->current_sampling = (engine->current_sampling + 1) % sampling_methods.size();
                engine->logical_device.waitIdle();

                if(sampling_methods[engine->current_sampling] == SamplingMethod::IMP_COL || sampling_methods[engine->current_sampling] == SamplingMethod::IMP_HIT){
                    engine->updateImportanceSampling();
                }
                else{
                    vmaDestroyBuffer(engine->vma_allocator, engine->sample_data_buffer.buffer, engine->sample_data_buffer.allocation);
                    vmaDestroyBuffer(engine->vma_allocator, engine->hit_data_buffer.buffer, engine->hit_data_buffer.allocation);
                    engine->createRayTracingDataBuffers();
                    engine -> createRayTracingDescriptorSets();
                    engine -> createPointCloudDescriptorSets();
                }
            }
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
        engine -> accumulation_frame = 0;
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

void Engine::createRTOutputImage() {
    // Create Image: Storage (for writing) | TransferSrc (for copy/blit) | TransferDst (for clear)
    vk::Format rt_format = vk::Format::eR32G32B32A32Sfloat;
    
    rt_output_image = Image::createImage(
        swapchain.extent.width, swapchain.extent.height,
        1, vk::SampleCountFlagBits::e1, 
        rt_format, // Or eR16G16B16A16Sfloat for HDR
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 
        *this
    );

    rt_output_image.image_view = Image::createImageView(rt_output_image, *this);

    // Usage: TransferDst (from RT image) | TransferSrc (to CPU buffer)
    capture_resolve_image = Image::createImage(
        swapchain.extent.width, swapchain.extent.height,
        1, vk::SampleCountFlagBits::e1, 
        // vk::Format::eR8G8B8A8Unorm, // Standard format for saving to disk (JPG/PNG)
        vk::Format::eB8G8R8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal, 
        *this
    );

    // Transition to General Layout (Ready for RayGen writing)
    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_graphics, &logical_device);
    transitionImage(cmd, rt_output_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);
    endSingleTimeCommands(cmd, graphics_queue);
}

// ------ Init Functions

bool Engine::initWindow(){
    uint32_t w = win_width;
    uint32_t h = win_height;
    window = initWindowGLFW("Engine", w, h);
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

bool Engine::initVulkan(int mssa_val){
    createInstance();
    setupDebugMessanger();
    createSurface();

    // Get device and queues
    physical_device = Device::pickPhysicalDevice(*this);
    logical_device = Device::createLogicalDevice(*this, queue_indices); 
    graphics_queue = Device::getQueue(*this, queue_indices.graphics_family.value());
    present_queue = Device::getQueue(*this, queue_indices.present_family.value());
    transfer_queue = Device::getQueue(*this, queue_indices.transfer_family.value());

    // Load Ray Tracing function pointers
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(logical_device.getProcAddr("vkGetAccelerationStructureBuildSizesKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(logical_device.getProcAddr("vkCreateAccelerationStructureKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(logical_device.getProcAddr("vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(logical_device.getProcAddr("vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(logical_device.getProcAddr("vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(logical_device.getProcAddr("vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(logical_device.getProcAddr("vkCmdTraceRaysKHR"));

    if (!vkGetAccelerationStructureBuildSizesKHR || !vkCreateAccelerationStructureKHR || 
        !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureDeviceAddressKHR || 
        !vkCreateRayTracingPipelinesKHR || !vkGetRayTracingShaderGroupHandlesKHR || !vkCmdTraceRaysKHR) {
        throw std::runtime_error("Failed to load ray tracing pipeline function pointers!");
    }
    
    // Get RT properties
    auto rt_pipeline_props = physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    rt_props.pipeline_props = rt_pipeline_props.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    
    auto as_props = physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
    rt_props.as_props = as_props.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

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

    createTlasResources();

    createRTOutputImage();

    Image::createDepthResources(physical_device, depth_image, swapchain.extent.width, swapchain.extent.height, *this);
    std::cout << "Memory usage after depth image creation" << std::endl;
    printGpuMemoryUsage();

    blue_noise_txt = Image::createTextureImage(*this, blue_noise_txt_path, vk::Format::eR8G8B8A8Srgb);
    blue_noise_txt_sampler = Image::createTextureSampler(physical_device, &logical_device, 1);
    std::cout << "Memory usage after blue noise creation" << std::endl;
    printGpuMemoryUsage();

    // PIPELINE CREATION
    createPipelines();
    createRayTracingPipeline();

    loadScene("main_scene.json");
    std::cout << "Memory status loading objects in scene" << std::endl;
    printGpuMemoryUsage();

    debug_cube = createDebugCube();


    createTorusModel();

    createRayTracingDataBuffers();

    createGlobalBindlessBuffers();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createShaderBindingTable();

    createGraphicsCommandBuffers();
    createSyncObjects();

    camera = Camera(swapchain.extent.width * 1.0 / swapchain.extent.height);
    
    prev_time = std::chrono::high_resolution_clock::now();

    std::cout << "Memory usage after initialization" << std::endl;
    printGpuMemoryUsage();

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

    createPointCloudPipeline();
}

void Engine::loadScene(const std::string &scene_path)
{
    // Open the scene file
    std::ifstream scene_file(scene_path);
    if (!scene_file.is_open()) {
        throw std::runtime_error("Failed to open scene file: " + scene_path);
    }
    // Parse the JSON data
    json scene_data = json::parse(scene_file);

    std::ifstream scene_to_load(scene_data.value("scene", ""));
    if(!scene_to_load.is_open()){
         throw std::runtime_error("Failed to open scene file: " + scene_data.value("scene", ""));
    }
    scene_data = json::parse(scene_to_load);



    // --- 1. Parse Settings ---
    std::string rtbox_path = "";

    if (scene_data.contains("settings")) {
        const auto& settings = scene_data["settings"];
        this->use_rt_box = settings.value("use_rt_box", false);
        rtbox_path = settings.value("rt_box_file", "");

        this -> render_torus = settings.value("render_torus", render_torus);
        this -> activate_point_cloud = settings.value("render_pointcloud", activate_point_cloud);

        if(settings.contains("ambient_light")){
            ubo.ambient_light = glm::vec4{
                settings["ambient_light"][0],
                settings["ambient_light"][1],
                settings["ambient_light"][2],
                settings["ambient_light"][3],
            };
        }
        else{
            ubo.ambient_light = glm::vec4(1.f);
        }

        if (settings.contains("torus_settings")) {
            const auto& t_set = settings["torus_settings"];
            torus_config.major_radius = t_set.value("major_radius", 16.0f);
            torus_config.minor_radius = t_set.value("minor_radius", 1.0f);
            torus_config.height = t_set.value("height", 8.0f);
            torus_config.major_segments = t_set.value("major_segments", 500);
            torus_config.minor_segments = t_set.value("minor_segments", 500);

            num_rays = t_set.value("num_rays", num_rays);
        }
    }

    // --- 2. Load Objects ---
    if (!scene_data.contains("objects")) {
        std::cout << "Warning: Scene file contains no 'objects' array." << std::endl;
        return;
    }

    // Iterate over each object definition in the JSON
    // Iterate over each object definition in the JSON
    for (const auto& obj_def : scene_data["objects"]) {
        P_object new_object;

        std::string model_path = obj_def["model"];
        new_object.loadModel(model_path, *this);
        
        // --- STEP 1: Apply JSON Transforms to generate the Model Matrix ---
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

        // --- STEP 2: Bake Transform into Vertices ---
        glm::mat4 transform = new_object.model_matrix;
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        for(auto& v : new_object.vertices) {
            // Transform Position
            v.pos = glm::vec3(transform * glm::vec4(v.pos, 1.0f));
            
            // Transform Normal
            v.normal = glm::normalize(normal_matrix * v.normal);
            
            // Transform Tangent (keep w component)
            glm::vec3 t = glm::vec3(v.tangent);
            t = glm::normalize(glm::mat3(transform) * t);
            v.tangent = glm::vec4(t, v.tangent.w);
        }

        float scale_factor = glm::length(glm::vec3(transform[0]));
        for(auto& l : new_object.local_lights) {
            // Transform Position
            l.position = glm::vec3(transform * glm::vec4(l.position, 1.0f));
            
            // Transform Direction (Rotation only)
            l.direction = glm::normalize(normal_matrix * l.direction);
            if (l.range > 0.0f) {
                l.range *= scale_factor;
            }
            l.intensity *= (scale_factor * scale_factor);
        }

        // --- STEP 3: Re-calculate Emissive Areas (Crucial for Lighting) ---
        // Since we potentially scaled the mesh, the pre-calculated triangle areas are wrong.
        for(auto& tri : new_object.emissive_triangles) {
            const auto& p0 = new_object.vertices[tri.index0].pos;
            const auto& p1 = new_object.vertices[tri.index1].pos;
            const auto& p2 = new_object.vertices[tri.index2].pos;
            tri.area = 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
        }

        // --- STEP 4: Reset Object Transform ---
        // The vertices are now at the final position. We must reset the object's
        // container transform to Identity, otherwise the TLAS will move them AGAIN.
        new_object.changePosition(glm::vec3(0.0f));
        new_object.changeRotation(glm::vec3(0.0f));
        new_object.changeScale(glm::vec3(1.0f));

        // --- STEP 5: Create Model (Uploads Baked World-Space Vertices) ---
        // Now the BLAS is built using the exact world coordinates.
        createModel(new_object);

        if (this->use_rt_box && !rtbox_path.empty()) {
            createRTBox(rtbox_path);
        } else if (this->use_rt_box && rtbox_path.empty()) {
            std::cerr << "Warning: 'use_rt_box' is true but no 'rt_box_file' was specified." << std::endl;
        }

        scene_objs.emplace_back(std::move(new_object));
    }
}

void Engine::createTorusModel()
{
    torus.generateMesh(
        torus_config.major_radius, 
        torus_config.minor_radius, 
        torus_config.height, 
        torus_config.major_segments, 
        torus_config.minor_segments
    );
    Primitive torusPrim;
    torusPrim.index_count = torus.indices.size();
    torusPrim.first_index = 0;
    torusPrim.material_index = 0;

    Material torusMat;
    torusMat.albedo_texture_index = -1; // No texture, shader should use baseColorFactor
    torusMat.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% transparent white
    
    torusMat.is_transparent = true; // Explicitly mark material as transparent

    torus.materials.push_back(std::move(torusMat));

    createModel(torus); 
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
    uint32_t rt_sets = MAX_FRAMES_IN_FLIGHT;
    uint32_t pc_sets = MAX_FRAMES_IN_FLIGHT;
    uint32_t total_sets = rt_sets + pc_sets;

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eUniformBuffer, total_sets },
        { vk::DescriptorType::eStorageBuffer, MAX_FRAMES_IN_FLIGHT * 11 },
        { vk::DescriptorType::eAccelerationStructureKHR, rt_sets },
        { vk::DescriptorType::eStorageImage, rt_sets },
        { vk::DescriptorType::eCombinedImageSampler, 
          MAX_FRAMES_IN_FLIGHT * (MAX_BINDLESS_TEXTURES)+MAX_FRAMES_IN_FLIGHT } 
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet; // Allow freeing if needed
    pool_info.maxSets = total_sets;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    descriptor_pool = vk::raii::DescriptorPool(logical_device, pool_info);
}

void Engine::createDescriptorSets()
{
    // 1. The Main Ray Tracing Set (binds TLAS, Global Textures, Output Image, etc.)
    createRayTracingDescriptorSets();

    // 2. The Point Cloud Overlay Set
    createPointCloudDescriptorSets();
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

void Engine::createPointCloudPipeline()
{
    // 1. Create Descriptor Set Layout
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        // Binding 0: Main UBO (view, proj)
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                       vk::ShaderStageFlagBits::eVertex, nullptr),
        // Binding 1: Hit Data Buffer
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                       vk::ShaderStageFlagBits::eVertex, nullptr),
        // --- cBinding 2: Torus Vertex Buffer ---
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr)
    };
    point_cloud_pipeline.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings);
    
    // 2. Create Pipeline
    point_cloud_pipeline.pipeline = Pipeline::createGraphicsPipeline(
        *this,
        &point_cloud_pipeline,
        v_shader_pointcloud,
        f_shader_pointcloud,
        TransparencyMode::POINTCLOUD,
        vk::CullModeFlagBits::eNone // No culling for points
    );
}

void Engine::createPointCloudDescriptorSets()
{
    point_cloud_descriptor_sets.clear();

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *point_cloud_pipeline.descriptor_set_layout);
    vk::DescriptorSetAllocateInfo alloc_info(
        *descriptor_pool,
        static_cast<uint32_t>(layouts.size()),
        layouts.data()
    );
    point_cloud_descriptor_sets = logical_device.allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // Binding 0: Main UBO
        vk::DescriptorBufferInfo ubo_info;
        ubo_info.buffer = uniform_buffers[i].buffer;
        ubo_info.offset = 0;
        ubo_info.range = sizeof(UniformBufferObject);

        vk::WriteDescriptorSet ubo_write;
        ubo_write.dstSet = *point_cloud_descriptor_sets[i];
        ubo_write.dstBinding = 0;
        ubo_write.descriptorType = vk::DescriptorType::eUniformBuffer;
        ubo_write.descriptorCount = 1;
        ubo_write.pBufferInfo = &ubo_info;

        // Binding 1: Hit Data Buffer
        vk::DescriptorBufferInfo hit_buffer_info;
        hit_buffer_info.buffer = hit_data_buffer.buffer;
        hit_buffer_info.offset = 0;
        hit_buffer_info.range = VK_WHOLE_SIZE;

        vk::WriteDescriptorSet hit_write;
        hit_write.dstSet = *point_cloud_descriptor_sets[i];
        hit_write.dstBinding = 1;
        hit_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        hit_write.descriptorCount = 1;
        hit_write.pBufferInfo = &hit_buffer_info;

        // --- FIX 2: Add Binding 2 (Torus Vertices) ---
        vk::DescriptorBufferInfo sampler_buffer_info;
        sampler_buffer_info.buffer = sample_data_buffer.buffer;
        sampler_buffer_info.offset = 0;
        sampler_buffer_info.range = VK_WHOLE_SIZE;

        vk::WriteDescriptorSet torus_write;
        torus_write.dstSet = *point_cloud_descriptor_sets[i];
        torus_write.dstBinding = 2;
        torus_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        torus_write.descriptorCount = 1;
        torus_write.pBufferInfo = &sampler_buffer_info;

        // Update writes array to size 3
        std::array<vk::WriteDescriptorSet, 3> writes = {ubo_write, hit_write, torus_write};
        logical_device.updateDescriptorSets(writes, {});
    }
}


void Engine::createGlobalBindlessBuffers()
{
    // --- 1. Create local vectors to aggregate data ---
    std::vector<MaterialPushConstant> global_materials_data;
    std::vector<Vertex> global_scene_vertices;
    std::vector<uint32_t> global_scene_indices;
    std::vector<MeshInfo> global_mesh_info;

    // --- NEW: Light Sampling Vectors ---
    std::vector<LightTriangle> global_light_triangles;
    std::vector<float> light_triangle_fluxes; // Temp storage for CDF build

    global_texture_descriptors.clear();

    int current_texture_offset = 0;

    global_punctual_lights.clear();
    
    // Helper lambda to aggregate a single game object
    auto aggregate_object = [&](Gameobject& obj) {
        if (obj.vertices.empty()) return;
        
        // Base offsets for this object
        uint32_t vertex_offset = static_cast<uint32_t>(global_scene_vertices.size());
        uint32_t index_offset = static_cast<uint32_t>(global_scene_indices.size());
        uint32_t material_offset = static_cast<uint32_t>(global_materials_data.size());
        
        obj.mesh_info_offset = static_cast<uint32_t>(global_mesh_info.size());

        // Process Textures
        for(auto& tex: obj.textures){
            vk::DescriptorImageInfo image_info;
            image_info.sampler = *obj.default_sampler;
            image_info.imageView = *tex.image_view;
            image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            global_texture_descriptors.push_back(image_info);
        }
        
        // Append Geometry
        global_scene_vertices.insert(global_scene_vertices.end(), obj.vertices.begin(), obj.vertices.end());
        global_scene_indices.insert(global_scene_indices.end(), obj.indices.begin(), obj.indices.end());

        // Append Materials
        for (const auto& mat : obj.materials) {
            MaterialPushConstant p_const = {};
            p_const.base_color_factor = mat.base_color_factor;
            p_const.emissive_factor_and_pad = glm::vec4(mat.emissive_factor, 0.0f);
            p_const.metallic_factor = mat.metallic_factor;
            p_const.roughness_factor = mat.roughness_factor;
            p_const.occlusion_strength = mat.occlusion_strength;
            p_const.specular_factor = mat.specular_factor;
            p_const.specular_color_factor = mat.specular_color_factor;
            p_const.alpha_cutoff = mat.alpha_cutoff;
            p_const.transmission_factor = mat.transmission_factor;
            p_const.clearcoat_factor = mat.clearcoat_factor;
            p_const.clearcoat_roughness_factor = mat.clearcoat_roughness_factor;
            p_const.albedo_texture_index = current_texture_offset + mat.albedo_texture_index;
            p_const.normal_texture_index = current_texture_offset + mat.normal_texture_index;
            p_const.metallic_roughness_texture_index = current_texture_offset + mat.metallic_roughness_texture_index;
            p_const.emissive_texture_index = current_texture_offset + mat.emissive_texture_index;
            p_const.occlusion_texture_index = current_texture_offset + mat.occlusion_texture_index;
            p_const.pad = mat.is_transparent ? 1.0f : 0.0f;
            p_const.clearcoat_texture_index = current_texture_offset + mat.clearcoat_texture_index;
            p_const.clearcoat_roughness_texture_index = current_texture_offset + mat.clearcoat_roughness_texture_index;
            p_const.sg_id = current_texture_offset + mat.specular_glossiness_texture_index;
            p_const.use_specular_glossiness_workflow = mat.use_specular_glossiness_workflow;
            p_const.uv_normal = mat.uv_normal;
            p_const.uv_emissive = mat.uv_emissive;
            p_const.uv_albedo = mat.uv_albedo;
            global_materials_data.push_back(p_const);
        }

        // Append MeshInfo
        for (const auto& prim : obj.o_primitives) {
            global_mesh_info.push_back({
                material_offset + prim.material_index,
                vertex_offset,
                index_offset + prim.first_index
            });
        }

        // --- NEW: Collect Emissive Triangles ---
        // We convert local indices to Global Absolute Indices using 'vertex_offset'
        for (const auto& tri : obj.emissive_triangles) {
            LightTriangle l_tri;
            l_tri.v0 = vertex_offset + tri.index0;
            l_tri.v1 = vertex_offset + tri.index1;
            l_tri.v2 = vertex_offset + tri.index2;
            l_tri.material_index = material_offset + tri.material_index;
            
            global_light_triangles.push_back(l_tri);

            // Calculate Flux (Power) = Area * Emission Strength (Length of color vector)
            // Note: We use the max component or length of emissive factor.
            // Ideally, we should look up the texture if it exists, but for CDF building, 
            // base factor is usually a good enough approximation for probability.
            const Material& mat = obj.materials[tri.material_index];
            float emission_strength = glm::length(mat.emissive_factor);
            
            // Flux = Area * Radiance (Approximation)
            // We assume Lambertian emission over the hemisphere (PI factor usually involved but cancels out in PDF)
            float flux = tri.area * emission_strength;
            light_triangle_fluxes.push_back(flux);
        }

        if (!obj.local_lights.empty()) {
            for(auto &l : obj.local_lights){
                if(l.intensity > 0.f){
                    global_punctual_lights.push_back(l);
                }
            }
        }

        current_texture_offset += obj.textures.size();
    };

    // --- 2. Aggregate all objects ---
    for (Gameobject& obj : scene_objs) {
        if(&obj != &torus)
            aggregate_object(obj);
    }
    // aggregate_object(debug_cube);
    if (use_rt_box) {
        aggregate_object(rt_box);
    }

    // --- 3. Build CDF ---
    std::vector<LightCDF> global_light_cdf;
    num_light_triangles = static_cast<uint32_t>(global_light_triangles.size());
    ubo.emissive_flux = 0.0f;

    if (num_light_triangles > 0) {
        // Calculate total flux
        for (float f : light_triangle_fluxes) ubo.emissive_flux += f;

        // Build CDF
        float running_sum = 0.0f;
        for (size_t i = 0; i < num_light_triangles; i++) {
            running_sum += light_triangle_fluxes[i];
            
            LightCDF cdf_entry;
            cdf_entry.cumulative_probability = (ubo.emissive_flux > 0.0f) ? (running_sum / ubo.emissive_flux) : 0.0f;
            cdf_entry.triangle_index = static_cast<uint32_t>(i);
            cdf_entry.padding[0] = 0.0f; cdf_entry.padding[1] = 0.0f;
            
            global_light_cdf.push_back(cdf_entry);
        }
        // Force last entry to 1.0 to prevent precision errors
        global_light_cdf.back().cumulative_probability = 1.0f;
    } else {
        // Dummy entry to prevent binding errors if no lights exist
        global_light_triangles.push_back({0, 0, 0, 0});
        global_light_cdf.push_back({1.0f, 0, {0.0f, 0.0f}});
    }

    ubo.punctual_flux = 0.f;
    if(global_punctual_lights.size() > 0){
        for (const auto& l : global_punctual_lights) {
            if (l.type == 1) { // Directional
                ubo.punctual_flux += l.intensity * 400.0f; // Assuming a scene cross-section of ~20x20m = 400m^2.
            } else { // Point or Spot
                // Flux (Lumens) = Intensity (Candela) * SolidAngle (4PI)
                ubo.punctual_flux += l.intensity * 12.566f; // 4 * PI
            }
        }
    }
    else{
        global_punctual_lights.emplace_back();
    }

    ubo.total_flux = ubo.emissive_flux + ubo.punctual_flux;
    if(ubo.emissive_flux > 0 && ubo.punctual_flux > 0){
        ubo.p_emissive = ubo.emissive_flux / ubo.total_flux;
        ubo.p_emissive = std::clamp(ubo.p_emissive, 0.1f, 0.9f);
    }


    auto upload_buffer = [&](AllocatedBuffer& buffer, vk::DeviceSize data_size, const void* data) {
        if (data_size == 0) return;
        
        AllocatedBuffer staging_buffer;
        createBuffer(vma_allocator, data_size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, staging_buffer);
        
        void* mapped;
        vmaMapMemory(vma_allocator, staging_buffer.allocation, &mapped);
        memcpy(mapped, data, (size_t)data_size);
        vmaUnmapMemory(vma_allocator, staging_buffer.allocation);
        
        createBuffer(vma_allocator, data_size,
                     vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                     vk::MemoryPropertyFlagBits::eDeviceLocal, buffer);
                     
        copyBuffer(staging_buffer.buffer, buffer.buffer, data_size, command_pool_graphics, &logical_device, graphics_queue);
    };
    
    upload_buffer(all_vertices_buffer, sizeof(Vertex) * global_scene_vertices.size(), global_scene_vertices.data());
    upload_buffer(all_indices_buffer, sizeof(uint32_t) * global_scene_indices.size(), global_scene_indices.data());
    upload_buffer(all_materials_buffer, sizeof(MaterialPushConstant) * global_materials_data.size(), global_materials_data.data());
    upload_buffer(all_mesh_info_buffer, sizeof(MeshInfo) * global_mesh_info.size(), global_mesh_info.data());

    // --- NEW: Upload Light Buffers ---
    upload_buffer(light_triangle_buffer, sizeof(LightTriangle) * global_light_triangles.size(), global_light_triangles.data());
    upload_buffer(light_cdf_buffer, sizeof(LightCDF) * global_light_cdf.size(), global_light_cdf.data());
    upload_buffer(punctual_light_buffer, sizeof(PunctualLight) * global_punctual_lights.size(), global_punctual_lights.data());
    
    std::cout << "Built Light CDF: " << num_light_triangles << " emissive triangles. Total Flux: " << ubo.emissive_flux << std::endl;
    std::cout << "Built punctual lights: " << global_punctual_lights.size() << " total lights. Total Flux: " << ubo.punctual_flux << std::endl;
}


// ------ Render Loop Functions

void Engine::run(){
    // --- Common SBT Setup ---
    auto align_up = [&](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
    handle_size = rt_props.pipeline_props.shaderGroupHandleSize;
    sbt_entry_size = align_up(handle_size, rt_props.pipeline_props.shaderGroupBaseAlignment);
    sbt_address = getBufferDeviceAddress(sbt_buffer.buffer);
    rmiss_region = vk::StridedDeviceAddressRegionKHR{sbt_address + 2 * sbt_entry_size, sbt_entry_size, 2 * sbt_entry_size};
    rhit_region = vk::StridedDeviceAddressRegionKHR{sbt_address + 4 * sbt_entry_size, sbt_entry_size, sbt_entry_size};
    callable_region= vk::StridedDeviceAddressRegionKHR{};

    while(!glfwWindowShouldClose(window)){
        accumulation_frame++;
        glfwPollEvents();
        drawFrame();
    }

    logical_device.waitIdle();

    cleanup();
}

void Engine::recordCommandBuffer(uint32_t image_index){
    auto& cmd = graphics_command_buffer[current_frame];
    cmd.begin({});

    // 1. Always Build TLAS (Required for ray queries in both modes)
    if(accumulation_frame == 0) // This is possible since we are using static scenes, remove for changing scenes
        buildTlas(cmd); 

    // Bind RT Pipeline & Descriptors (Shared by both modes)
    cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline.layout, 0, {*rt_descriptor_sets[current_frame]}, {});
    
    // Push Constants (Shared)
    RayPushConstant p_const;
    p_const.model = torus.model_matrix;
    p_const.major_radius = torus.getMajorRadius();
    p_const.minor_radius = torus.getMinorRadius();
    p_const.height = torus.getHeight();
    cmd.pushConstants<RayPushConstant>(*rt_pipeline.layout, vk::ShaderStageFlagBits::eRaygenKHR, 0, p_const);


    // =========================================================
    // MODE A: POINT CLOUD ANALYSIS (Pressed 'P')
    // =========================================================
    if(render_point_cloud)
    {
        // 1. Run Data Capture Pass (RayGen 0: Torus -> Scene)
        // This calculates positions and colors using the updated raygen.rgen
        vk::StridedDeviceAddressRegionKHR rgen_torus{sbt_address + 0 * sbt_entry_size, sbt_entry_size, sbt_entry_size};
        uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(sampling_points.size())));
        
        vkCmdTraceRaysKHR(*cmd, rgen_torus, rmiss_region, rhit_region, callable_region, side, side, 1);
        
        // Barrier: Ensure RT is done writing to hit_buffer before Vertex Shader reads it
        vk::MemoryBarrier2 mem_barrier;
        mem_barrier.srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
        mem_barrier.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
        mem_barrier.dstStageMask = vk::PipelineStageFlagBits2::eVertexShader; 
        mem_barrier.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead;
        vk::DependencyInfo dep_info;
        dep_info.memoryBarrierCount = 1;
        dep_info.pMemoryBarriers = &mem_barrier;
        cmd.pipelineBarrier2(dep_info);

        // 2. Prepare Swapchain for Rasterization (Clear Screen)
        transition_image_layout(image_index, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                                {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                                vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput);

        vk::RenderingAttachmentInfo color_att{};
        color_att.imageView = swapchain.image_views[image_index];
        color_att.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        color_att.loadOp = vk::AttachmentLoadOp::eClear; // Clear previous frame
        color_att.storeOp = vk::AttachmentStoreOp::eStore;
        color_att.clearValue = vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}; // Black Background

        vk::RenderingAttachmentInfo depth_att{};
        depth_att.imageView = depth_image.image_view;
        depth_att.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
        depth_att.loadOp = vk::AttachmentLoadOp::eClear;
        depth_att.storeOp = vk::AttachmentStoreOp::eDontCare;
        depth_att.clearValue = vk::ClearDepthStencilValue(1.0f, 0);

        vk::RenderingInfo render_info{};
        render_info.renderArea.extent = swapchain.extent;
        render_info.layerCount = 1;
        render_info.colorAttachmentCount = 1;
        render_info.pColorAttachments = &color_att;
        render_info.pDepthAttachment = &depth_att;

        cmd.beginRendering(render_info);
        cmd.setViewport(0, vk::Viewport(0.f, 0.f, (float)swapchain.extent.width, (float)swapchain.extent.height, 0.f, 1.f));
        cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain.extent));
        cmd.setCullMode(vk::CullModeFlagBits::eNone); // Required for dynamic state

        // Draw Point Cloud
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *point_cloud_pipeline.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *point_cloud_pipeline.layout, 0, {*point_cloud_descriptor_sets[current_frame]}, {});
        
        PC pc_data;
        pc_data.model = torus.model_matrix;
        pc_data.major_radius = torus.getMajorRadius();
        pc_data.minor_radius = torus.getMinorRadius();
        pc_data.height = torus.getHeight();

        // --- Toggle 'O': Show Scene Point Cloud ---
        if (render_final_pointcloud) {
            pc_data.mode = 0; // World Hit
            cmd.pushConstants<PC>(*point_cloud_pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, pc_data);
            cmd.draw(static_cast<uint32_t>(sampling_points.size()), 1, 0, 0);
        }

        // --- Toggle 'T': Show Projected Torus Surface ---
        if (show_projected_torus) {
            pc_data.mode = 1; // Torus UV Surface
            cmd.pushConstants<PC>(*point_cloud_pipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, pc_data);
            cmd.draw(static_cast<uint32_t>(sampling_points.size()), 1, 0, 0);
        }

        cmd.endRendering();

        // Transition to Present
        transition_image_layout(image_index, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
                                vk::AccessFlagBits2::eColorAttachmentWrite, {},
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);
    }
    // =========================================================
    // MODE B: STANDARD RENDER (Normal View)
    // =========================================================
    else 
    {
        // 1. Run Camera View Pass (RayGen 1: Camera -> Scene)
        vk::StridedDeviceAddressRegionKHR rgen_camera{sbt_address + 1 * sbt_entry_size, sbt_entry_size, sbt_entry_size};
        vkCmdTraceRaysKHR(*cmd, rgen_camera, rmiss_region, rhit_region, callable_region, swapchain.extent.width, swapchain.extent.height, 1);
        
        // 2. Transition & Copy to Swapchain
        vk::ImageMemoryBarrier2 image_barrier;
        image_barrier.srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
        image_barrier.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
        image_barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
        image_barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        image_barrier.oldLayout = vk::ImageLayout::eGeneral;
        image_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        image_barrier.image = rt_output_image.image;
        image_barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::ImageMemoryBarrier2 swap_barrier;
        swap_barrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        swap_barrier.srcAccessMask = {};
        swap_barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
        swap_barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        swap_barrier.oldLayout = vk::ImageLayout::eUndefined;
        swap_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        swap_barrier.image = swapchain.images[image_index];
        swap_barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        vk::ImageMemoryBarrier2 barriers[] = {image_barrier, swap_barrier};
        vk::DependencyInfo dep_info;
        dep_info.imageMemoryBarrierCount = 2;
        dep_info.pImageMemoryBarriers = barriers;
        cmd.pipelineBarrier2(dep_info);

        vk::ImageBlit blitRegion;
        blitRegion.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1};
        blitRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };

        blitRegion.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1};
        blitRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };

        cmd.blitImage(
            rt_output_image.image,            // Source (Float32)
            vk::ImageLayout::eTransferSrcOptimal,
            swapchain.images[image_index],    // Dest (Int8 / Swapchain)
            vk::ImageLayout::eTransferDstOptimal,
            blitRegion,                       // <--- FIXED: Pass the object directly (implicit ArrayProxy)
            vk::Filter::eNearest
        );

        // Restore RT Image Layout
        transitionImage(cmd, rt_output_image.image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);

        // Transition Swapchain to Present
        transition_image_layout(image_index, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR,
                                vk::AccessFlagBits2::eTransferWrite, {},
                                vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eBottomOfPipe);
    }

    cmd.end();
}

void Engine::drawFrame(){
    while( vk::Result::eTimeout == logical_device.waitForFences(*in_flight_fences[current_frame], vk::True, UINT64_MAX));
    
    if (is_capturing) {
        captureSceneData();
    }

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
        logical_device.waitIdle();
        vmaDestroyBuffer(vma_allocator, torus.geometry_buffer.buffer, torus.geometry_buffer.allocation);
        createModel(torus); // TODO 0001 -> maybe there is a better way to manage this
        updateTorusRTBuffer();
    }
    // Normal Camera Mode accumulation logic
    if (input.move != glm::vec2(0) || input.look_x != 0 || input.look_y != 0 || changed) {
        accumulation_frame = 0;
    }
    camera.update(time, input, torus.getMajorRadius(), torus.getHeight());
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
    // --- 1. Update Camera Matrices ---
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    
    if(camera.getCurrentState().is_toroidal)
        ubo.camera_pos = camera.getCurrentState().t_camera.position;
    else
        ubo.camera_pos = camera.getCurrentState().f_camera.position;

    ubo.frame_count = accumulation_frame;
    ubo.fov = glm::radians(camera.getCurrentState().fov);
    ubo.height = win_height;

    // --- 6. Copy Data to GPU ---
    memcpy(uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
}

/**
 * @brief Creates persistent buffers for building the TLAS each frame.
 * This includes the instance buffer, the scratch buffer, and the TLAS object itself.
 */
void Engine::createTlasResources()
{
    // 1. Create the Instance Buffer (Host Visible)
    vk::DeviceSize instance_buffer_size = sizeof(vk::AccelerationStructureInstanceKHR) * MAX_TLAS_INSTANCES;
    
    createBuffer(vma_allocator, instance_buffer_size,
                 vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 tlas_instance_buffer);
    
    // Map it permanently
    vmaMapMemory(vma_allocator, tlas_instance_buffer.allocation, &tlas_instance_buffer_mapped);

    // Get its device address
    uint64_t instance_buffer_addr = getBufferDeviceAddress(tlas_instance_buffer.buffer);

    // 2. Get Build Sizes for the TLAS
    // We do a dummy setup to get the required buffer sizes
    
    vk::AccelerationStructureGeometryInstancesDataKHR instances_data;
    instances_data.arrayOfPointers = vk::False;
    instances_data.data.deviceAddress = instance_buffer_addr;

    vk::AccelerationStructureGeometryKHR tlas_geometry;
    tlas_geometry.geometryType = vk::GeometryTypeKHR::eInstances;
    tlas_geometry.geometry.instances = instances_data;
    tlas_geometry.flags = vk::GeometryFlagBitsKHR::eOpaque; // Instances are considered opaque

    vk::AccelerationStructureBuildGeometryInfoKHR build_info;
    build_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    build_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate; // <-- Allow updates
    build_info.geometryCount = 1;
    build_info.pGeometries = &tlas_geometry;

    uint32_t primitive_count = MAX_TLAS_INSTANCES;
    vk::AccelerationStructureBuildSizesInfoKHR size_info;
    vkGetAccelerationStructureBuildSizesKHR(
        *logical_device,
        static_cast<VkAccelerationStructureBuildTypeKHR>(vk::AccelerationStructureBuildTypeKHR::eDevice),
        reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(&build_info),
        &primitive_count,
        reinterpret_cast<VkAccelerationStructureBuildSizesInfoKHR*>(&size_info)
    );

    // 3. Create TLAS Buffer
    createBuffer(vma_allocator, size_info.accelerationStructureSize,
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 tlas.buffer);

    // 4. Create TLAS Object
    vk::AccelerationStructureCreateInfoKHR as_create_info;
    as_create_info.buffer = tlas.buffer.buffer;
    as_create_info.size = size_info.accelerationStructureSize;
    as_create_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;

    VkAccelerationStructureKHR vk_as;
    if (vkCreateAccelerationStructureKHR(*logical_device, reinterpret_cast<const VkAccelerationStructureCreateInfoKHR*>(&as_create_info), nullptr, &vk_as) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TLAS object!");
    }
    tlas.as = vk::raii::AccelerationStructureKHR(logical_device, vk_as);

    // 5. Create persistent Scratch Buffer
    createBuffer(vma_allocator, size_info.buildScratchSize,
                 vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 tlas_scratch_buffer);
    tlas_scratch_addr = getBufferDeviceAddress(tlas_scratch_buffer.buffer);

    // 6. Get the TLAS device address
    vk::AccelerationStructureDeviceAddressInfoKHR addr_info(*tlas.as);
    tlas.device_address = logical_device.getAccelerationStructureAddressKHR(addr_info);
}

/**
 * @brief Builds the Top-Level Acceleration Structure (TLAS) for the current frame.
 * This collects all valid BLASs, updates their transforms, and rebuilds the TLAS.
 */
void Engine::buildTlas(vk::raii::CommandBuffer& cmd)
{
    // 1. Gather all object instances
    std::vector<vk::AccelerationStructureInstanceKHR> instances;

    auto gather_instances = [&](Gameobject& obj) {
        if (obj.blas.as == nullptr) return; // Skip objects without a BLAS (like the torus)
        
        vk::AccelerationStructureInstanceKHR instance;
        
        // The transform matrix is row-major. GLM is column-major. So we transpose.
        glm::mat4 transp_model = glm::transpose(obj.model_matrix);
        memcpy(&instance.transform, &transp_model, sizeof(vk::TransformMatrixKHR));
        
        instance.instanceCustomIndex = obj.mesh_info_offset;
        instance.mask = 0xFF; // All rays hit
        instance.instanceShaderBindingTableRecordOffset = 0; // Only one hit group
        instance.flags = static_cast<VkGeometryInstanceFlagBitsKHR>(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);
        instance.accelerationStructureReference = obj.blas.device_address;
        
        instances.push_back(instance);
    };

    for (Gameobject& obj : scene_objs) {
        if(&obj != &torus)
            gather_instances(obj);
    }
    // gather_instances(debug_cube);
    if (use_rt_box) {
        gather_instances(rt_box);
    }

    if (instances.empty()) {
        return; // Nothing to build
    }

    // 2. Copy instance data to the host-visible buffer
    memcpy(tlas_instance_buffer_mapped, instances.data(), instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));

    // 3. Add barrier: Wait for CPU buffer write to be visible to the GPU
    vk::MemoryBarrier2 mem_barrier;
    mem_barrier.srcStageMask = vk::PipelineStageFlagBits2::eHost;
    mem_barrier.srcAccessMask = vk::AccessFlagBits2::eHostWrite;
    mem_barrier.dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    mem_barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    cmd.pipelineBarrier2(vk::DependencyInfo({}, 1, &mem_barrier, 0, nullptr, 0, nullptr));

    // 4. Set up the build info
    vk::AccelerationStructureGeometryInstancesDataKHR instances_data;
    instances_data.arrayOfPointers = vk::False;
    instances_data.data.deviceAddress = getBufferDeviceAddress(tlas_instance_buffer.buffer);

    vk::AccelerationStructureGeometryKHR tlas_geometry;
    tlas_geometry.geometryType = vk::GeometryTypeKHR::eInstances;
    tlas_geometry.geometry.instances = instances_data;
    tlas_geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;

    vk::AccelerationStructureBuildGeometryInfoKHR build_info;
    build_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    build_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    build_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild; // Re-build every frame
    build_info.dstAccelerationStructure = *tlas.as;
    build_info.geometryCount = 1;
    build_info.pGeometries = &tlas_geometry;
    build_info.scratchData.deviceAddress = tlas_scratch_addr;

    vk::AccelerationStructureBuildRangeInfoKHR range_info;
    range_info.primitiveCount = static_cast<uint32_t>(instances.size());
    range_info.primitiveOffset = 0;
    range_info.firstVertex = 0;
    range_info.transformOffset = 0;
    
    const VkAccelerationStructureBuildRangeInfoKHR* p_build_range = 
        reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR*>(&range_info);
    const VkAccelerationStructureBuildRangeInfoKHR* const p_build_range_const_ptr = p_build_range;

    // 5. Record the build command
    vkCmdBuildAccelerationStructuresKHR(
        *cmd, 
        1, 
        reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(&build_info), 
        &p_build_range_const_ptr
    );

    // 6. Add barrier: Wait for TLAS build to finish before any ray tracing
    vk::MemoryBarrier2 build_barrier;
    build_barrier.srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
    build_barrier.srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR;
    build_barrier.dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR; // For the *next* step
    build_barrier.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR;
    cmd.pipelineBarrier2(vk::DependencyInfo({}, 1, &build_barrier, 0, nullptr, 0, nullptr));
}

/**
 * @brief Creates the SSBOs for ray tracing data collection.
 * This includes the input buffer (torus vertices) and output buffer (hit data).
 * Must be called *after* createTorusModel().
 */
void Engine::createRayTracingDataBuffers()
{
    // --- 1. Generate Samples based on selected method ---
    switch(sampling_methods[current_sampling]){
        case SamplingMethod::HALTON:
            Sampling::generateHaltonSamples(sampling_points, num_rays);
            break;
        case SamplingMethod::LHS:
            Sampling::generateLatinHypercubeSamples(sampling_points, num_rays);
            break;
        case SamplingMethod::STRATIFIED:
            Sampling::generateStratifiedSamples(sampling_points, num_rays);
            break;
        case SamplingMethod::RANDOM:
            Sampling::generateRandomSamples(sampling_points, num_rays);
            break;
        case SamplingMethod::UNIFORM:
            Sampling::generateUniformSamples(sampling_points, num_rays);
            break;
        case SamplingMethod::IMP_COL:
        case SamplingMethod::IMP_HIT:
            // Fallback to Halton for the FIRST frame of importance sampling
            // (Since we have no previous data yet to importance-sample from)
            if (sampling_points.empty()) {
                Sampling::generateHaltonSamples(sampling_points, num_rays);
            }
            break;
    }

    if (sampling_points.empty()) {
        std::cerr << "Error: No sampling points generated!" << std::endl;
        return;
    }

    // --- 2. Create Input Buffer (Ray Samples) ---
    // This buffer holds the UV coordinates that the RayGen shader uses to spawn rays on the torus.
    vk::DeviceSize sample_size = sizeof(RaySample) * sampling_points.size();

    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, sample_size,
                vk::BufferUsageFlagBits::eTransferSrc,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);

    void* data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, sampling_points.data(), (size_t)sample_size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    createBuffer(vma_allocator, sample_size,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                sample_data_buffer);
    
    copyBuffer(staging_buffer.buffer, sample_data_buffer.buffer, sample_size,
                command_pool_graphics, &logical_device, graphics_queue);

    // --- 3. Create Output Buffer (Hit Data) ---
    // This buffer stores the results (color, position, hit flag) from the Torus Capture RayGen.
    vk::DeviceSize hit_data_size = sizeof(HitDataGPU) * sampling_points.size();

    createBuffer(vma_allocator, hit_data_size,
                 vk::BufferUsageFlagBits::eStorageBuffer | 
                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eTransferSrc | 
                 vk::BufferUsageFlagBits::eTransferDst, // Added TransferDst to allow clearing/filling if needed
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 hit_data_buffer);
}

void Engine::createRayTracingPipeline()
{
    // --- 1. DESCRIPTOR SET LAYOUT ---
    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    bindings.emplace_back(0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR, nullptr);
    bindings.emplace_back(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR, nullptr);
    bindings.emplace_back(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR, nullptr);
    bindings.emplace_back(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR, nullptr);
    bindings.emplace_back(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR, nullptr);
    bindings.emplace_back(5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR, nullptr);
    bindings.emplace_back(6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR, nullptr);
    bindings.emplace_back(7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR, nullptr);

    // Binding 8: Light Triangles
    bindings.emplace_back(8, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR, nullptr);
    // Binding 9: Light CDF
    bindings.emplace_back(9, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR, nullptr);

    // Binding 10: Output Image
    bindings.emplace_back(10, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eRaygenKHR, nullptr);

    bindings.emplace_back(11, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eRaygenKHR, nullptr);

    bindings.emplace_back(12, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eClosestHitKHR, nullptr);

    // Binding 11: Textures (Must be LAST)
    bindings.emplace_back(13, vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(MAX_BINDLESS_TEXTURES), 
                          vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR, nullptr);

    // --- FLAGS ---
    // Size is now 12 (Indices 0 to 11)
    std::vector<vk::DescriptorBindingFlags> binding_flags(bindings.size(), vk::DescriptorBindingFlags{});
    binding_flags.back() = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

    vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info;
    flags_info.bindingCount = static_cast<uint32_t>(binding_flags.size());
    flags_info.pBindingFlags = binding_flags.data();

    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.pNext = &flags_info;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    rt_pipeline.descriptor_set_layout = Pipeline::createDescriptorSetLayout(*this, bindings, layout_info);
    // --- 2. SHADER MODULES ---
    // We need to load 5 shaders now.
    
    // Group 0: RayGen (Torus Capture)
    vk::raii::ShaderModule rgen_torus = Pipeline::createShaderModule(Pipeline::readFile(rt_rgen_shader), &logical_device);
    
    // Group 1: RayGen (Camera View)
    vk::raii::ShaderModule rgen_camera = Pipeline::createShaderModule(Pipeline::readFile("shaders/rt_datacollect/raygen_camera.rgen.spv"), &logical_device);
    
    // Group 2: Miss (Primary - Sky/Background)
    vk::raii::ShaderModule rmiss_primary = Pipeline::createShaderModule(Pipeline::readFile(rt_rmiss_shader), &logical_device);
    
    // Group 3: Miss (Shadow - Visibility)
    vk::raii::ShaderModule rmiss_shadow = Pipeline::createShaderModule(Pipeline::readFile("shaders/rt_datacollect/shadow.rmiss.spv"), &logical_device);
    
    // Group 4: Closest Hit (PBR + Shadows)
    vk::raii::ShaderModule rchit = Pipeline::createShaderModule(Pipeline::readFile(rt_rchit_shader), &logical_device);

    vk::raii::ShaderModule ranyhit = Pipeline::createShaderModule(Pipeline::readFile("shaders/rt_datacollect/alpha.rahit.spv"), &logical_device);

    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    stages.push_back({{}, vk::ShaderStageFlagBits::eRaygenKHR, *rgen_torus, "main"});      // 0
    stages.push_back({{}, vk::ShaderStageFlagBits::eRaygenKHR, *rgen_camera, "main"});     // 1
    stages.push_back({{}, vk::ShaderStageFlagBits::eMissKHR, *rmiss_primary, "main"});     // 2
    stages.push_back({{}, vk::ShaderStageFlagBits::eMissKHR, *rmiss_shadow, "main"});      // 3
    stages.push_back({{}, vk::ShaderStageFlagBits::eClosestHitKHR, *rchit, "main"});       // 4
    stages.push_back({{}, vk::ShaderStageFlagBits::eAnyHitKHR, *ranyhit, "main"});

    // --- 3. SHADER GROUPS ---
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;

    // Group 0: RayGen Torus
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});
    
    // Group 1: RayGen Camera
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});
    
    // Group 2: Miss Primary
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});
    
    // Group 3: Miss Shadow
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});
    
    // Group 4: Hit Group (Triangle Closest Hit)
    groups.push_back({
        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, 
        VK_SHADER_UNUSED_KHR, // General
        4,                    // Closest Hit
        5,                    // Any Hit <--- ATTACH HERE
        VK_SHADER_UNUSED_KHR  // Intersection
    });

    // --- 4. PIPELINE LAYOUT ---
    // Push constants needed for Torus Model Matrix & Geometry info
    vk::PushConstantRange push_constant;
    push_constant.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR; // Used by both RayGens
    push_constant.offset = 0;
    push_constant.size = sizeof(RayPushConstant);

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &*rt_pipeline.descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant;

    rt_pipeline.layout = vk::raii::PipelineLayout(logical_device, pipeline_layout_info);

    // --- 5. PIPELINE CREATION ---
    vk::RayTracingPipelineCreateInfoKHR pipeline_info;
    pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.groupCount = static_cast<uint32_t>(groups.size());
    pipeline_info.pGroups = groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = 2; // 1 for primary, 2 for reflection/shadow recursion
    pipeline_info.layout = *rt_pipeline.layout;

    VkPipeline vk_pipeline;
    VkResult res = vkCreateRayTracingPipelinesKHR(
        *logical_device,
        VK_NULL_HANDLE, // Deferred Operation
        VK_NULL_HANDLE, // Pipeline Cache
        1, pipeline_info,
        nullptr, &vk_pipeline
    );

    if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline!");
    }

    rt_pipeline.pipeline = vk::raii::Pipeline(logical_device, vk_pipeline);
}

void Engine::createRayTracingDescriptorSets()
{
    rt_descriptor_sets.clear();

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *rt_pipeline.descriptor_set_layout);
    
    vk::DescriptorSetAllocateInfo alloc_info(
        *descriptor_pool,
        static_cast<uint32_t>(layouts.size()),
        layouts.data()
    );

    // Variable count applies to the last binding (Binding 11) automatically
    std::vector<uint32_t> variable_counts(MAX_FRAMES_IN_FLIGHT, MAX_BINDLESS_TEXTURES);
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_alloc_info;
    variable_alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    variable_alloc_info.pDescriptorCounts = variable_counts.data();
    
    alloc_info.pNext = &variable_alloc_info;

    rt_descriptor_sets = logical_device.allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<vk::WriteDescriptorSet> writes;

        // --- Binding 0: TLAS ---
        vk::WriteDescriptorSetAccelerationStructureKHR as_info;
        as_info.accelerationStructureCount = 1;
        as_info.pAccelerationStructures = &(*tlas.as);

        vk::WriteDescriptorSet as_write;
        as_write.dstSet = *rt_descriptor_sets[i];
        as_write.dstBinding = 0;
        as_write.dstArrayElement = 0;
        as_write.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        as_write.descriptorCount = 1;
        as_write.pNext = &as_info;
        writes.push_back(as_write);

        // --- Binding 1: Sample Buffer ---
        vk::DescriptorBufferInfo sample_info(sample_data_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet sample_write;
        sample_write.dstSet = *rt_descriptor_sets[i];
        sample_write.dstBinding = 1;
        sample_write.dstArrayElement = 0;
        sample_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        sample_write.descriptorCount = 1;
        sample_write.pBufferInfo = &sample_info;
        writes.push_back(sample_write);

        // --- Binding 2: Hit Buffer ---
        vk::DescriptorBufferInfo hit_info(hit_data_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet hit_write;
        hit_write.dstSet = *rt_descriptor_sets[i];
        hit_write.dstBinding = 2;
        hit_write.dstArrayElement = 0;
        hit_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        hit_write.descriptorCount = 1;
        hit_write.pBufferInfo = &hit_info;
        writes.push_back(hit_write);

        // --- Binding 3: Materials Buffer ---
        vk::DescriptorBufferInfo mat_info(all_materials_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet mat_write;
        mat_write.dstSet = *rt_descriptor_sets[i];
        mat_write.dstBinding = 3;
        mat_write.dstArrayElement = 0;
        mat_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        mat_write.descriptorCount = 1;
        mat_write.pBufferInfo = &mat_info;
        writes.push_back(mat_write);

        // --- Binding 4: Uniform Buffer ---
        vk::DescriptorBufferInfo ubo_info(uniform_buffers[i].buffer, 0, sizeof(UniformBufferObject));
        vk::WriteDescriptorSet ubo_write;
        ubo_write.dstSet = *rt_descriptor_sets[i];
        ubo_write.dstBinding = 4;
        ubo_write.dstArrayElement = 0;
        ubo_write.descriptorType = vk::DescriptorType::eUniformBuffer;
        ubo_write.descriptorCount = 1;
        ubo_write.pBufferInfo = &ubo_info;
        writes.push_back(ubo_write);

        // --- Binding 5: Vertices ---
        vk::DescriptorBufferInfo vert_info(all_vertices_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet vert_write;
        vert_write.dstSet = *rt_descriptor_sets[i];
        vert_write.dstBinding = 5;
        vert_write.dstArrayElement = 0;
        vert_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        vert_write.descriptorCount = 1;
        vert_write.pBufferInfo = &vert_info;
        writes.push_back(vert_write);

        // --- Binding 6: Indices ---
        vk::DescriptorBufferInfo idx_info(all_indices_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet idx_write;
        idx_write.dstSet = *rt_descriptor_sets[i];
        idx_write.dstBinding = 6;
        idx_write.dstArrayElement = 0;
        idx_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        idx_write.descriptorCount = 1;
        idx_write.pBufferInfo = &idx_info;
        writes.push_back(idx_write);

        // --- Binding 7: Mesh Info ---
        vk::DescriptorBufferInfo mesh_info(all_mesh_info_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet mesh_write;
        mesh_write.dstSet = *rt_descriptor_sets[i];
        mesh_write.dstBinding = 7;
        mesh_write.dstArrayElement = 0;
        mesh_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        mesh_write.descriptorCount = 1;
        mesh_write.pBufferInfo = &mesh_info;
        writes.push_back(mesh_write);

        // --- Binding 8: Light Triangles ---
        vk::DescriptorBufferInfo l_tri_info(light_triangle_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet l_tri_write;
        l_tri_write.dstSet = *rt_descriptor_sets[i];
        l_tri_write.dstBinding = 8;
        l_tri_write.dstArrayElement = 0;
        l_tri_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        l_tri_write.descriptorCount = 1;
        l_tri_write.pBufferInfo = &l_tri_info;
        writes.push_back(l_tri_write);

        // --- Binding 9: Light CDF ---
        vk::DescriptorBufferInfo l_cdf_info(light_cdf_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet l_cdf_write;
        l_cdf_write.dstSet = *rt_descriptor_sets[i];
        l_cdf_write.dstBinding = 9;
        l_cdf_write.dstArrayElement = 0;
        l_cdf_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        l_cdf_write.descriptorCount = 1;
        l_cdf_write.pBufferInfo = &l_cdf_info;
        writes.push_back(l_cdf_write);

        // --- Binding 10: Output Image (Shifted) ---
        vk::DescriptorImageInfo storage_image_info;
        storage_image_info.imageView = *rt_output_image.image_view;
        storage_image_info.imageLayout = vk::ImageLayout::eGeneral;

        vk::WriteDescriptorSet output_write;
        output_write.dstSet = *rt_descriptor_sets[i];
        output_write.dstBinding = 10;
        output_write.dstArrayElement = 0;
        output_write.descriptorType = vk::DescriptorType::eStorageImage;
        output_write.descriptorCount = 1;
        output_write.pImageInfo = &storage_image_info; // Pointer to info
        writes.push_back(output_write);

        vk::WriteDescriptorSet noise_write;
        noise_write.dstSet = *rt_descriptor_sets[i];
        noise_write.dstBinding = 11;
        noise_write.dstArrayElement = 0;
        noise_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        noise_write.descriptorCount = 1;
        blue_noise_txt_info.sampler = *blue_noise_txt_sampler;
        blue_noise_txt_info.imageView = *blue_noise_txt.image_view;
        blue_noise_txt_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        noise_write.pImageInfo = &blue_noise_txt_info;
        writes.push_back(noise_write);

        vk::DescriptorBufferInfo light_info(punctual_light_buffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet light_write;
        light_write.dstSet = *rt_descriptor_sets[i];
        light_write.dstBinding = 12;
        light_write.dstArrayElement = 0;
        light_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        light_write.descriptorCount = 1;
        light_write.pBufferInfo = &light_info;
        writes.push_back(light_write);

        // --- Binding 13: Global Textures (Last) ---
        if (!global_texture_descriptors.empty()) {
            vk::WriteDescriptorSet texture_write;
            texture_write.dstSet = *rt_descriptor_sets[i];
            texture_write.dstBinding = 13;
            texture_write.dstArrayElement = 0;
            texture_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            texture_write.descriptorCount = static_cast<uint32_t>(global_texture_descriptors.size());
            texture_write.pImageInfo = global_texture_descriptors.data();
            writes.push_back(texture_write);
        }
        
        logical_device.updateDescriptorSets(writes, {});
    }
}

void Engine::createShaderBindingTable()
{
    // 1. Get properties
    uint32_t handle_size = rt_props.pipeline_props.shaderGroupHandleSize;
    uint32_t sbt_entry_alignment = rt_props.pipeline_props.shaderGroupBaseAlignment;
    
    // Helper lambda to align a size
    auto align_up = [&](uint32_t size, uint32_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    };
    
    // The size of each entry in the SBT must be the handle size, aligned up to the BASE alignment
    uint32_t sbt_entry_size = align_up(handle_size, sbt_entry_alignment);
    
    // We have 5 groups total
    uint32_t group_count = 5;

    // Total SBT size
    vk::DeviceSize sbt_size = group_count * sbt_entry_size;

    // 2. Create SBT Buffer (Host Visible)
    createBuffer(vma_allocator, sbt_size,
                 vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 sbt_buffer);

    // 3. Get the shader group handles from the pipeline
    std::vector<uint8_t> shader_handles(group_count * handle_size);
    
    VkResult res = vkGetRayTracingShaderGroupHandlesKHR(
        *logical_device,
        *rt_pipeline.pipeline,
        0, // firstGroup
        group_count, // groupCount
        shader_handles.size(), // dataSize
        shader_handles.data() // pData
    );
    if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to get ray tracing shader group handles!");
    }

    // 4. Map and copy handles into the SBT with padding/alignment
    void* sbt_mapped;
    vmaMapMemory(vma_allocator, sbt_buffer.allocation, &sbt_mapped);
    
    uint8_t* p_data = static_cast<uint8_t*>(sbt_mapped);
    
    for(uint32_t i = 0; i < group_count; i++) {
        // Source: Packed tightly in shader_handles (stride = handle_size)
        // Dest: Aligned in SBT buffer (stride = sbt_entry_size)
        memcpy(p_data + (i * sbt_entry_size), 
               shader_handles.data() + (i * handle_size), 
               handle_size);
    }
    
    vmaUnmapMemory(vma_allocator, sbt_buffer.allocation);
}

void Engine::recreateSwapChain(){
    glfwGetFramebufferSize(window, &win_width, &win_height);
    
    // Pause while minimized
    while (win_width == 0 || win_height == 0){
        glfwGetFramebufferSize(window, &win_width, &win_height);
        glfwWaitEvents();
    }

    logical_device.waitIdle();

    // 1. Recreate Swapchain
    swapchain.image_views.clear();
    swapchain.swapchain = nullptr; // Release old swapchain
    swapchain = Swapchain::createSwapChain(*this);

    // 2. Recreate Depth Image (Used by Point Cloud overlay)
    // Destroy old depth image
    if (depth_image.image) {
        vmaDestroyImage(vma_allocator, depth_image.image, depth_image.allocation);
    }
    Image::createDepthResources(physical_device, depth_image, swapchain.extent.width, swapchain.extent.height, *this);

    // 3. Recreate Ray Tracing Output Image
    // This helper (defined earlier) destroys the old image and creates a new one 
    // with the new swapchain dimensions.
    if(rt_output_image.image){
        vmaDestroyImage(vma_allocator, rt_output_image.image, rt_output_image.allocation);
    }
    if(capture_resolve_image.image){
        vmaDestroyImage(vma_allocator, capture_resolve_image.image, capture_resolve_image.allocation);
    }
    createRTOutputImage();

    // 4. Update Descriptor Sets
    // The RT Descriptor Set (Binding 10) holds the 'rt_output_image' view.
    // Since the view has changed, we must update the descriptors.
    createDescriptorSets();

    // 5. Update Camera
    camera.modAspectRatio(swapchain.extent.width * 1.0 / swapchain.extent.height);

    std::cout << "Swapchain recreated: " << swapchain.extent.width << "x" << swapchain.extent.height << std::endl;
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

    if (rt_output_image.image) {
         vmaDestroyImage(vma_allocator, rt_output_image.image, rt_output_image.allocation);
    }
    if (capture_resolve_image.image) {
         vmaDestroyImage(vma_allocator, capture_resolve_image.image, capture_resolve_image.allocation);
    }
    

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

void Engine::updateTorusRTBuffer()
{
    vk::DeviceSize vertex_data_size = sizeof(Vertex) * torus.vertices.size();

    // Check if the existing buffer is large enough.
    if (torus_vertex_data_buffer.buffer == VK_NULL_HANDLE) {
        return;
    }

    // Create a temporary staging buffer
    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, vertex_data_size,
                 vk::BufferUsageFlagBits::eTransferSrc,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 staging_buffer);

    // Map memory and copy the NEW vertex data
    void* data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, torus.vertices.data(), (size_t)vertex_data_size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    // Copy from Staging -> Existing Device Local Buffer
    // This updates the data effectively "moving" the rays source
    copyBuffer(staging_buffer.buffer, torus_vertex_data_buffer.buffer, vertex_data_size,
               command_pool_graphics, &logical_device, graphics_queue);

    // Staging buffer destructor cleans itself up here
}


ImageReadbackData Engine::readImageToCPU(vk::Image image, VkFormat format, uint32_t width, uint32_t height) {
    vk::DeviceSize image_size = width * height * 4;

    // 1. Create Staging Buffer
    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, image_size,
                 vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 staging_buffer);

    // 2. Copy Command
    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_graphics, &logical_device);

    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};
    
    cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, staging_buffer.buffer, {region});
    endSingleTimeCommands(cmd, graphics_queue);

    // 3. Read & Swizzle
    ImageReadbackData read_data;
    read_data.width = width;
    read_data.height = height;
    read_data.data.resize((size_t)image_size);
    
    void* mapped_data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &mapped_data);
    memcpy(read_data.data.data(), mapped_data, (size_t)image_size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    // --- FIX: SWIZZLE BGR -> RGB ---
    // Check if the format implies BGR. Most Swapchains are B8G8R8A8.
    // Even if we are not 100% sure of the enum, checking for the 'B' component first is safe
    // or simply forcing the swap if you observe the artifact.
    bool is_bgr = (format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SNORM);

    if (is_bgr) {
        for (size_t i = 0; i < read_data.data.size(); i += 4) {
            std::swap(read_data.data[i], read_data.data[i + 2]); // Swap Blue (0) and Red (2)
        }
    }

    return read_data;
}

void Engine::captureSceneData() {
    const int ACCUMULATION_STEPS = 2048; 
    const int TOTAL_POSITIONS = 336; 

    std::cout << "\n--- Starting Dataset Capture ---" << std::endl;
    std::cout << "1. Capturing " << TOTAL_POSITIONS << " Camera Views (from inside Torus)" << std::endl;
    std::cout << "2. Generating Point Cloud Data" << std::endl;

    logical_device.waitIdle();

    // --- SETUP ---
    // Save original state
    bool original_cloud_state = activate_point_cloud;
    // Turn OFF point cloud rendering for the images. 
    // We want the "Normal Scene" as seen by a camera inside the torus.
    activate_point_cloud = false; 

    std::vector<FrameData> recorded_frames;
    std::vector<FrameData> test_frames;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> alpha_dist(0.0f, 360.0f);
    std::uniform_real_distribution<> beta_dist(-45.f, 45.f);

    // Common SBT setup
    auto align_up = [&](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
    uint32_t handle_size = rt_props.pipeline_props.shaderGroupHandleSize;
    uint32_t sbt_entry_size = align_up(handle_size, rt_props.pipeline_props.shaderGroupBaseAlignment);
    uint64_t sbt_address = getBufferDeviceAddress(sbt_buffer.buffer);

    // Shader regions
    vk::StridedDeviceAddressRegionKHR rmiss_region{sbt_address + 2 * sbt_entry_size, sbt_entry_size, 2 * sbt_entry_size};
    vk::StridedDeviceAddressRegionKHR rhit_region{sbt_address + 4 * sbt_entry_size, sbt_entry_size, sbt_entry_size};
    vk::StridedDeviceAddressRegionKHR callable_region{};

    // =========================================================
    // PHASE 1: CAMERA IMAGES (View from INSIDE Torus)
    // =========================================================
    for (int i = 0; i < TOTAL_POSITIONS; ++i) {
        float alpha = alpha_dist(gen);
        float beta = beta_dist(gen);
        
        // 1. Position Camera INSIDE the torus
        // This updates the View Matrix in the UBO
        camera.updateToroidalAngles(alpha, beta, torus.getMajorRadius(), torus.getHeight());

        // 2. Accumulate 5000 Frames for this Viewpoint
        for (int frame = 0; frame < ACCUMULATION_STEPS; ++frame) {
            
            // Update UBO (Frame Count is critical for mixing: 1/1, 1/2, 1/3...)
            accumulation_frame = frame;
            updateUniformBuffer(current_frame); 

            vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_graphics, &logical_device);

            // Rebuild TLAS (Scene might be dynamic, or just to be safe)
            if(frame == 0)
                buildTlas(cmd);

            // Bind RT Pipeline
            cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline.pipeline);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline.layout, 0, {*rt_descriptor_sets[current_frame]}, {});

            // Push Constants (Camera/Model transforms)
            RayPushConstant p_const;
            p_const.model = torus.model_matrix;
            p_const.major_radius = torus.getMajorRadius();
            p_const.minor_radius = torus.getMinorRadius();
            p_const.height = torus.getHeight();
            cmd.pushConstants<RayPushConstant>(*rt_pipeline.layout, vk::ShaderStageFlagBits::eRaygenKHR, 0, p_const);

            // --- TRACE CAMERA RAYS ONLY ---
            // RayGen Index 1: "raygen_camera.rgen"
            vk::StridedDeviceAddressRegionKHR rgen_camera{sbt_address + 1 * sbt_entry_size, sbt_entry_size, sbt_entry_size};
            
            vkCmdTraceRaysKHR(*cmd, 
                rgen_camera, // Use Camera Shader
                rmiss_region, 
                rhit_region, 
                callable_region, 
                swapchain.extent.width, swapchain.extent.height, 1);

            endSingleTimeCommands(cmd, graphics_queue);
        }

        // 3. Save the Resulting Image
        // (Float32 Accumulation Buffer -> Int8 Staging Buffer)
        vk::raii::CommandBuffer cmd_copy = beginSingleTimeCommands(command_pool_graphics, &logical_device);

        transitionImage(cmd_copy, capture_resolve_image.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor);
        transitionImage(cmd_copy, rt_output_image.image, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor);

        vk::ImageBlit blitRegion;
        blitRegion.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blitRegion.srcOffsets[1] = vk::Offset3D{static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1};
        blitRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        blitRegion.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blitRegion.dstOffsets[1] = vk::Offset3D{static_cast<int32_t>(swapchain.extent.width), static_cast<int32_t>(swapchain.extent.height), 1};
        blitRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };

        cmd_copy.blitImage(
            rt_output_image.image, vk::ImageLayout::eTransferSrcOptimal,
            capture_resolve_image.image, vk::ImageLayout::eTransferDstOptimal,
            blitRegion, vk::Filter::eNearest
        );

        // Reset RT Image for next loop (back to General)
        transitionImage(cmd_copy, rt_output_image.image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);
        transitionImage(cmd_copy, capture_resolve_image.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor);

        endSingleTimeCommands(cmd_copy, graphics_queue);

        // Readback and Save
        ImageReadbackData data = readImageToCPU(capture_resolve_image.image, capture_resolve_image.image_format, swapchain.extent.width, swapchain.extent.height);
        
        // Downscale loop (same as before)...
        uint32_t target_w = data.width / 2;
        uint32_t target_h = data.height / 2;
        std::vector<uint8_t> resized_pixels(target_w * target_h * 4);
        for (uint32_t y = 0; y < target_h; ++y) {
            for (uint32_t x = 0; x < target_w; ++x) {
                uint32_t src_idx = ((y * 2) * data.width + (x * 2)) * 4;
                uint32_t dst_idx = (y * target_w + x) * 4;
                resized_pixels[dst_idx + 0] = data.data[src_idx + 0]; 
                resized_pixels[dst_idx + 1] = data.data[src_idx + 1]; 
                resized_pixels[dst_idx + 2] = data.data[src_idx + 2]; 
                resized_pixels[dst_idx + 3] = 255; 
            }
        }
        data.width = target_w;
        data.height = target_h;
        data.data = resized_pixels;

        std::string full_save_path = "dataset/train/r_" + std::to_string(i) + ".jpg";
        saveJPG(full_save_path, data);

        // Metadata
        FrameData frame_data;
        frame_data.file_path = "./train/r_" + std::to_string(i); 
        frame_data.transform_matrix = glm::inverse(camera.getViewMatrix()); // Camera-to-World
        
        if (i % 4 == 0) test_frames.push_back(frame_data);
        else recorded_frames.push_back(frame_data);

        std::cout << "Captured Image " << i + 1 << "/" << TOTAL_POSITIONS << " (" << ACCUMULATION_STEPS << " samples)\r" << std::flush;
    }
    std::cout << std::endl << "Images saved. Now generating Point Cloud..." << std::endl;

    // =========================================================
    // PHASE 2: POINT CLOUD GENERATION (Rays FROM Torus Surface)
    // =========================================================
    // We run this ONCE, but accumulate 5000 samples per point to remove noise from the PLY.
    
    for (int frame = 0; frame < ACCUMULATION_STEPS; ++frame) {
        
        accumulation_frame = frame;
        updateUniformBuffer(current_frame); 

        vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool_graphics, &logical_device);

        if(frame == 0)
            buildTlas(cmd);

        cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline.layout, 0, {*rt_descriptor_sets[current_frame]}, {});

        RayPushConstant p_const;
        p_const.model = torus.model_matrix;
        p_const.major_radius = torus.getMajorRadius();
        p_const.minor_radius = torus.getMinorRadius();
        p_const.height = torus.getHeight();
        cmd.pushConstants<RayPushConstant>(*rt_pipeline.layout, vk::ShaderStageFlagBits::eRaygenKHR, 0, p_const);

        // --- TRACE TORUS RAYS ONLY ---
        // RayGen Index 0: "raygen.rgen" (The Torus Surface sampler)
        vk::StridedDeviceAddressRegionKHR rgen_torus{sbt_address + 0 * sbt_entry_size, sbt_entry_size, sbt_entry_size};
        uint32_t side = static_cast<uint32_t>(std::ceil(std::sqrt(sampling_points.size())));
        
        vkCmdTraceRaysKHR(*cmd, 
            rgen_torus, // Use Torus Shader
            rmiss_region, 
            rhit_region, 
            callable_region, 
            side, side, 1);
            
        endSingleTimeCommands(cmd, graphics_queue);
        
        if (frame % 100 == 0) std::cout << "Accumulating Point Cloud: " << frame << "/" << ACCUMULATION_STEPS << "\r" << std::flush;
    }
    std::cout << std::endl;

    // 4. Save Metadata & PLY
    saveTransformsJson("dataset/transforms_train.json", recorded_frames);
    saveTransformsJson("dataset/transforms_test.json", test_frames);
    savePly("dataset/points3d.ply"); // Saves data from the filled hit_data_buffer

    // Restore state
    activate_point_cloud = original_cloud_state;
    is_capturing = false;
    std::cout << "--- Dataset Generation Complete ---" << std::endl;
}
void Engine::saveTransformsJson(const std::string& filename, const std::vector<FrameData>& frames) {
    json root;
    
    // 1. Camera Angle X (Horizontal FOV in radians)
    // 1. Get Vertical FOV from Camera (in Radians)
    float fov_y = glm::radians(camera.getCurrentState().fov);
    
    // 2. Get Aspect Ratio from Swapchain (Width / Height)
    // Note: Ensure you use the resolution of the SAVED IMAGES (e.g. if you resized to 960x526)
    // Assuming your camera aspect ratio matches your saved image aspect ratio:
    float aspect = camera.getCurrentState().aspect_ratio;

    // 3. Convert to Horizontal FOV (camera_angle_x)
    // Formula: tan(fovX / 2) = aspect * tan(fovY / 2)
    float fov_x = 2.0f * atan(tan(fov_y / 2.0f) * aspect);

    root["camera_angle_x"] = fov_x;

    // 2. Frames
    root["frames"] = json::array();
    for (const auto& frame : frames) {
        json frame_json;
        frame_json["file_path"] = frame.file_path;
        
        // Convert glm::mat4 to 4x4 array
        frame_json["transform_matrix"] = json::array();
        for (int row = 0; row < 4; ++row) {
            json row_arr = json::array();
            for (int col = 0; col < 4; ++col) {
                // GLM is column-major, but JSON expects row-major reading usually, 
                // effectively we just need to write out the rows.
                row_arr.push_back(frame.transform_matrix[col][row]); 
            }
            frame_json["transform_matrix"].push_back(row_arr);
        }
        
        root["frames"].push_back(frame_json);
    }

    std::ofstream file(filename);
    file << root.dump(4); // Pretty print
    std::cout << "Saved transforms to: " << filename << std::endl;
}

void Engine::savePly(const std::string& filename) {
    std::cout << "Exporting PLY..." << std::endl;

    // 1. Read Hit Buffer from GPU
    size_t num_rays = sampling_points.size();
    std::vector<HitDataGPU> hits(num_rays);
    readBuffer(hit_data_buffer.buffer, sizeof(HitDataGPU) * num_rays, hits.data());

    // 2. Filter valid points
    std::vector<HitDataGPU> valid_points;
    for (const auto& hit : hits) {
        if (hit.flag > 0.0f) { // Check for hit
            valid_points.push_back(hit);
        }
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return;
    }

    // 3. Write PLY Header
    file << "ply\n";
    file << "format ascii 1.0\n";
    file << "element vertex " << valid_points.size() << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    file << "property float nx\n";
    file << "property float ny\n";
    file << "property float nz\n";
    file << "property uchar red\n";
    file << "property uchar green\n";
    file << "property uchar blue\n";
    file << "end_header\n";

    // 2. Write Data
    for (const auto& p : valid_points) {
        int r = static_cast<int>(p.r * 255.0f);
        int g = static_cast<int>(p.g * 255.0f);
        int b = static_cast<int>(p.b * 255.0f);
        
        file << p.px << " " << p.py << " " << p.pz << " " 
             << p.nx << " " << p.ny << " " << p.nz << " "  // <--- Use REAL normals
             << r << " " << g << " " << b << "\n";
    }

    file.close();
    std::cout << "Saved " << valid_points.size() << " points to " << filename << std::endl;
}