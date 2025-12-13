#include "pipeline.h"

vk::raii::Pipeline Pipeline::createGraphicsPipeline( 
    PipelineInfo *p_info, 
    std::string v_shader, 
    std::string f_shader,
    TransparencyMode mode,
    vk::CullModeFlagBits cull_mode,
    vk::raii::Device &logical_device,
    vk::raii::PhysicalDevice &physical_device,
    vk::Format &swapchain_format)
{
    // --- 1. Shaders ---
    vk::raii::ShaderModule vertex_shader_module = createShaderModule(readFile(v_shader), logical_device);
    vk::raii::ShaderModule frag_shader_module = createShaderModule(readFile(f_shader), logical_device);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info({}, vk::ShaderStageFlagBits::eVertex, *vertex_shader_module, "main");
    vk::PipelineShaderStageCreateInfo frag_shader_stage_info({}, vk::ShaderStageFlagBits::eFragment, *frag_shader_module, "main");

    vk::PipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    // --- 2. Vertex Input ---
    // PointCloud (and modern GPU-driven rendering) uses SSBOs for vertex data, 
    // so we don't bind vertex buffers in the traditional way.
    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
    auto binding_description = Vertex::getBindingDescription();
    auto attribute_descriptions = Vertex::getAttributeDescriptions();

    if (mode == TransparencyMode::POINTCLOUD) {
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr;
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr;
    } else {
        // Fallback for standard raster (if added back later)
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_description; 
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data(); 
    }
    
    // --- 3. Input Assembly ---
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    if (mode == TransparencyMode::POINTCLOUD) {
        input_assembly.topology = vk::PrimitiveTopology::ePointList;
    } else {
        input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    }
    
    // --- 4. Viewport (Dynamic) ---
    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // --- 5. Rasterizer ---
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.cullMode = cull_mode;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False,
    rasterizer.lineWidth = 1.f;

    // --- 6. Multisampling ---
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1; // No multisampling basically, since the pointcloud displays points
    multisampling.sampleShadingEnable = vk::False;

    // --- 7. Depth Stencil ---
    vk::PipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.depthTestEnable = vk::True;
    depth_stencil.depthWriteEnable = vk::True; // Point cloud writes depth
    depth_stencil.depthBoundsTestEnable = vk::False;
    depth_stencil.stencilTestEnable = vk::False;
    depth_stencil.depthCompareOp = vk::CompareOp::eLess;

    // --- 8. Color Blending ---
    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = vk::False;
    color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = vk::False;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // --- 9. Dynamic State ---
    std::vector dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
        vk::DynamicState::eCullMode
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // --- 10. Push Constants & Layout ---
    std::vector<vk::PushConstantRange> push_constant_ranges;

    if (mode == TransparencyMode::POINTCLOUD) {
        // Point Cloud Push Constants (PC struct)
        vk::PushConstantRange pc_range;
        pc_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
        pc_range.offset = 0;
        pc_range.size = sizeof(PC);
        push_constant_ranges.push_back(pc_range);
    } else {
        // Standard Model/Material Push Constants (Fallback)
        vk::PushConstantRange vert_push;
        vert_push.stageFlags = vk::ShaderStageFlagBits::eVertex;
        vert_push.offset = 0;
        vert_push.size = sizeof(glm::mat4);
        push_constant_ranges.push_back(vert_push);

        vk::PushConstantRange frag_push;
        frag_push.stageFlags = vk::ShaderStageFlagBits::eFragment;
        frag_push.offset = sizeof(glm::mat4);
        frag_push.size = sizeof(MaterialPushConstant);
        push_constant_ranges.push_back(frag_push);
    }

    // --- Layout Creation ---
    // We removed the OIT referencing code here.
    // The layout comes purely from the p_info passed in (Engine sets this up).
    vk::PipelineLayoutCreateInfo pipeline_layout_info; 
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &*(p_info -> descriptor_set_layout);
    
    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

    p_info -> layout = vk::raii::PipelineLayout(logical_device, pipeline_layout_info);

    // --- 11. Rendering Info ---
    vk::Format depth_format = findDepthFormat(physical_device);
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;
    
    // We render to Swapchain + Depth
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats = &swapchain_format;
    pipeline_rendering_create_info.depthAttachmentFormat = depth_format;

    // --- 12. Pipeline Creation ---
    vk::GraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.pNext = &pipeline_rendering_create_info;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = *(p_info -> layout);
    pipeline_info.pDepthStencilState = &depth_stencil;

    return std::move(vk::raii::Pipeline(logical_device, nullptr, pipeline_info));
}

vk::raii::ShaderModule Pipeline::createShaderModule(const std::vector<char>& code, vk::raii::Device& logical_device){
    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vk::raii::ShaderModule shader_module{ logical_device, create_info };
    return shader_module;
}

vk::raii::Pipeline Pipeline::createRayTracingPipeline(
    PipelineInfo* p_info, 
    vk::raii::Device &logical_device, 
    const std::string& rgen_torus_path,
    const std::string& rgen_camera_path,
    const std::string& rmiss_primary_path,
    const std::string& rmiss_shadow_path,
    const std::string& rchit_path,
    const std::string& rahit_path,
    uint32_t push_constant_size,
    PFN_vkCreateRayTracingPipelinesKHR &vkCreateRayTracingPipelinesKHR)
{
    // 1. Load Shaders
    vk::raii::ShaderModule rgen_torus = createShaderModule(readFile(rgen_torus_path), logical_device);
    vk::raii::ShaderModule rgen_camera = createShaderModule(readFile(rgen_camera_path), logical_device);
    vk::raii::ShaderModule rmiss_primary = createShaderModule(readFile(rmiss_primary_path), logical_device);
    vk::raii::ShaderModule rmiss_shadow = createShaderModule(readFile(rmiss_shadow_path), logical_device);
    vk::raii::ShaderModule rchit = createShaderModule(readFile(rchit_path), logical_device);
    vk::raii::ShaderModule ranyhit = createShaderModule(readFile(rahit_path), logical_device);

    std::vector<vk::PipelineShaderStageCreateInfo> stages;

    // Stage 0: RayGen (Torus Capture)
    stages.push_back({{}, vk::ShaderStageFlagBits::eRaygenKHR, *rgen_torus, "main"});     
    // Stage 1: RayGen (Camera View)
    stages.push_back({{}, vk::ShaderStageFlagBits::eRaygenKHR, *rgen_camera, "main"});    
    // Stage 2: Miss (Primary - Sky/Background)
    stages.push_back({{}, vk::ShaderStageFlagBits::eMissKHR, *rmiss_primary, "main"});    
    // Stage 3: Miss (Shadow - Visibility)
    stages.push_back({{}, vk::ShaderStageFlagBits::eMissKHR, *rmiss_shadow, "main"});     
    // Stage 4: Closest Hit (PBR + Shadows)
    stages.push_back({{}, vk::ShaderStageFlagBits::eClosestHitKHR, *rchit, "main"});      
    // Stage 5: Any Hit (Alpha testing)
    stages.push_back({{}, vk::ShaderStageFlagBits::eAnyHitKHR, *ranyhit, "main"});

    // 2. Define Shader Groups
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;
    
    // Group 0: RayGen Torus (Uses Stage 0)
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    // Group 1: RayGen Camera (Uses Stage 1)
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    // Group 2: Miss Primary (Uses Stage 2)
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    // Group 3: Miss Shadow (Uses Stage 3)
    groups.push_back({vk::RayTracingShaderGroupTypeKHR::eGeneral, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR});

    // Group 4: Hit Group (Triangle Closest Hit + Any Hit) (Uses Stage 4 and 5)
    groups.push_back({
        vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, 
        VK_SHADER_UNUSED_KHR, // General
        4,                    // Closest Hit index
        5,                    // Any Hit index
        VK_SHADER_UNUSED_KHR  // Intersection
    });

    // 3. Create Pipeline Layout
    vk::PushConstantRange push_constant;
    push_constant.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;
    push_constant.offset = 0;
    push_constant.size = push_constant_size;

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &*(p_info->descriptor_set_layout); 
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant;

    p_info->layout = vk::raii::PipelineLayout(logical_device, pipeline_layout_info);

    // 4. Create the Pipeline
    vk::RayTracingPipelineCreateInfoKHR pipeline_info;
    pipeline_info.stageCount = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.groupCount = static_cast<uint32_t>(groups.size());
    pipeline_info.pGroups = groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = 2; // 1 for primary, 2 for reflection/shadow recursion
    pipeline_info.layout = *(p_info->layout);

    // Call the C function pointer
    VkPipeline vk_pipeline;
    VkResult res = vkCreateRayTracingPipelinesKHR(
        *logical_device,
        VK_NULL_HANDLE, // deferredOperation
        VK_NULL_HANDLE, // pipelineCache
        1, // createInfoCount
        reinterpret_cast<const VkRayTracingPipelineCreateInfoKHR*>(&pipeline_info),
        nullptr, // pAllocator
        &vk_pipeline
    );

    if (res != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline!");
    }

    return vk::raii::Pipeline(logical_device, vk_pipeline);
}
vk::raii::DescriptorSetLayout Pipeline::createDescriptorSetLayout(std::vector<vk::DescriptorSetLayoutBinding> &bindings, vk::raii::Device &logical_device)
{
    vk::DescriptorSetLayoutCreateInfo layout_info({}, bindings.size(), bindings.data());

    return std::move(vk::raii::DescriptorSetLayout(logical_device, layout_info));
}

vk::raii::DescriptorSetLayout Pipeline::createDescriptorSetLayout(std::vector<vk::DescriptorSetLayoutBinding> &bindings, vk::raii::Device &logical_device, vk::DescriptorSetLayoutCreateInfo &layout_info)
{
    return std::move(vk::raii::DescriptorSetLayout(logical_device, layout_info));
}

std::vector<char> Pipeline::readFile(const std::string& filename){
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open()){
        throw std::runtime_error("failed to open file: " + filename);
    }
    size_t file_size = (size_t) file.tellg();
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();
    return buffer;
}