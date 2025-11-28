#include "pipeline.h"
#include "engine.h"
#include <fstream>

vk::raii::Pipeline Pipeline::createGraphicsPipeline(
    Engine &engine, 
    PipelineInfo *p_info, 
    std::string v_shader, 
    std::string f_shader,
    TransparencyMode mode,
    vk::CullModeFlagBits cull_mode)
{
    // --- 1. Shaders ---
    vk::raii::ShaderModule vertex_shader_module = createShaderModule(readFile(v_shader), &engine.logical_device);
    vk::raii::ShaderModule frag_shader_module = createShaderModule(readFile(f_shader), &engine.logical_device);

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
    multisampling.rasterizationSamples = engine.mssa_samples;
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

    p_info -> layout = vk::raii::PipelineLayout(engine.logical_device, pipeline_layout_info);

    // --- 11. Rendering Info ---
    vk::Format depth_format = findDepthFormat(engine.physical_device);
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;
    
    // We render to Swapchain + Depth
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats = &engine.swapchain.format;
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

    return std::move(vk::raii::Pipeline(engine.logical_device, nullptr, pipeline_info));
}

vk::raii::ShaderModule Pipeline::createShaderModule(const std::vector<char>& code, vk::raii::Device * logical_device){
    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vk::raii::ShaderModule shader_module{ *logical_device, create_info };
    return shader_module;
}

vk::raii::Pipeline Pipeline::createRayTracingPipeline(Engine& engine, PipelineInfo* p_info, const std::string &rt_rgen_shader, const std::string &rt_rmiss_shader, const std::string &rt_rchit_shader)
{
    // 1. Load Shaders
    vk::raii::ShaderModule rgen_module = createShaderModule(readFile(rt_rgen_shader), &engine.logical_device);
    vk::raii::ShaderModule rmiss_module = createShaderModule(readFile(rt_rmiss_shader), &engine.logical_device);
    vk::raii::ShaderModule rchit_module = createShaderModule(readFile(rt_rchit_shader), &engine.logical_device);

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

    // Shader Stage 0: RayGen
    shader_stages.push_back(vk::PipelineShaderStageCreateInfo(
        {}, vk::ShaderStageFlagBits::eRaygenKHR, *rgen_module, "main"
    ));
    // Shader Stage 1: Miss
    shader_stages.push_back(vk::PipelineShaderStageCreateInfo(
        {}, vk::ShaderStageFlagBits::eMissKHR, *rmiss_module, "main"
    ));
    // Shader Stage 2: Closest Hit
    shader_stages.push_back(vk::PipelineShaderStageCreateInfo(
        {}, vk::ShaderStageFlagBits::eClosestHitKHR, *rchit_module, "main"
    ));

    // 2. Define Shader Groups
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_groups;
    
    // Group 0: RayGen Group
    vk::RayTracingShaderGroupCreateInfoKHR rgen_group;
    rgen_group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    rgen_group.generalShader = 0; // Index into shader_stages
    rgen_group.closestHitShader = VK_SHADER_UNUSED_KHR;
    rgen_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    rgen_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(rgen_group);

    // Group 1: Miss Group
    vk::RayTracingShaderGroupCreateInfoKHR miss_group;
    miss_group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    miss_group.generalShader = 1; // Index into shader_stages
    miss_group.closestHitShader = VK_SHADER_UNUSED_KHR;
    miss_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    miss_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(miss_group);

    // Group 2: Hit Group (Triangles)
    vk::RayTracingShaderGroupCreateInfoKHR chit_group;
    chit_group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    chit_group.generalShader = VK_SHADER_UNUSED_KHR;
    chit_group.closestHitShader = 2; // Index into shader_stages
    chit_group.anyHitShader = VK_SHADER_UNUSED_KHR;
    chit_group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shader_groups.push_back(chit_group);

    // 3. Create Pipeline Layout
    // We only have one push constant: the torus model matrix
    vk::PushConstantRange push_constant_range;
    push_constant_range.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(RayPushConstant);

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &*(p_info->descriptor_set_layout); // Use the layout we stored
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    p_info->layout = vk::raii::PipelineLayout(engine.logical_device, pipeline_layout_info);

    // 4. Create the Pipeline
    vk::RayTracingPipelineCreateInfoKHR pipeline_info;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.groupCount = static_cast<uint32_t>(shader_groups.size());
    pipeline_info.pGroups = shader_groups.data();
    pipeline_info.maxPipelineRayRecursionDepth = 1; // No recursion needed
    pipeline_info.layout = *(p_info->layout);

    // Call the C function pointer
    VkPipeline vk_pipeline;
    VkResult res = engine.vkCreateRayTracingPipelinesKHR(
        *engine.logical_device,
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

    return vk::raii::Pipeline(engine.logical_device, vk_pipeline);
}

vk::raii::DescriptorSetLayout Pipeline::createDescriptorSetLayout(Engine &engine, std::vector<vk::DescriptorSetLayoutBinding> &bindings)
{
    vk::DescriptorSetLayoutCreateInfo layout_info({}, bindings.size(), bindings.data());

    return std::move(vk::raii::DescriptorSetLayout(engine.logical_device, layout_info));
}

vk::raii::DescriptorSetLayout Pipeline::createDescriptorSetLayout(Engine &engine, std::vector<vk::DescriptorSetLayoutBinding> &bindings, vk::DescriptorSetLayoutCreateInfo &layout_info)
{
    return std::move(vk::raii::DescriptorSetLayout(engine.logical_device, layout_info));
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