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
    vk::raii::ShaderModule vertex_shader_module = createShaderModule(readFile(v_shader), &engine.logical_device);
    vk::raii::ShaderModule frag_shader_module = createShaderModule(readFile(f_shader), &engine.logical_device);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_shader_stage_info.module = *vertex_shader_module;
    vert_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
    frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_shader_stage_info.module = *frag_shader_module;
    frag_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info, frag_shader_stage_info
    };

    auto binding_description = Vertex::getBindingDescription();
    auto attribute_descriptions = Vertex::getAttributeDescriptions();
    
    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
    if (mode == TransparencyMode::OIT_COMPOSITE || mode == TransparencyMode::POINTCLOUD) {
        // Fullscreen quad has no vertex inputs
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr; // Be explicit
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr; // Be explicit
    } else {
        // Standard PBR/OIT object
        // Now we just assign the pointers, since the variables
        // are in the outer scope and will not be destroyed.
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_description; 
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()); // Good practice to cast
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data(); 
    }
    
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    if (mode == TransparencyMode::POINTCLOUD) {
        input_assembly.topology = vk::PrimitiveTopology::ePointList;
    } else {
        input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    }
    
    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.cullMode = cull_mode;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False,
    rasterizer.lineWidth = 1.f;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = engine.mssa_samples;
    if (mode == TransparencyMode::OIT_WRITE || mode == TransparencyMode::OIT_COMPOSITE) {
        // OIT Write AND Composite passes MUST run per-sample.
        multisampling.sampleShadingEnable = vk::True;
        multisampling.minSampleShading = 1.0f; // Force per-sample
    } else {
        // OPAQUE pass
        multisampling.sampleShadingEnable = vk::False;
    }

    vk::PipelineDepthStencilStateCreateInfo depth_stencil = {};
    if(mode == TransparencyMode::OIT_COMPOSITE){
        depth_stencil.depthTestEnable = vk::False;
        depth_stencil.depthWriteEnable = vk::False;
    } else{
        depth_stencil.depthTestEnable = vk::True;
        depth_stencil.depthWriteEnable = (mode == TransparencyMode::OPAQUE || mode == TransparencyMode::POINTCLOUD) ? vk::True : vk::False;
        depth_stencil.depthBoundsTestEnable = vk::False;
        depth_stencil.stencilTestEnable = vk::False;
        depth_stencil.depthCompareOp = vk::CompareOp::eLess;
    }

    vk::PipelineColorBlendAttachmentState color_blend_attachment; // For Opawaue/OIT_COMPOSITE

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = vk::False;

    switch(mode){
        case TransparencyMode::OPAQUE:
        { 
            // OPAQUE has 2 attachments: Lit and Albedo
            vk::PipelineColorBlendAttachmentState lit_attachment;
            lit_attachment.blendEnable = vk::False;
            lit_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            
            vk::PipelineColorBlendAttachmentState albedo_attachment = lit_attachment; // Copy
            
            static std::array<vk::PipelineColorBlendAttachmentState, 2> blend_attachments = {
                lit_attachment, albedo_attachment
            };

            color_blending.attachmentCount = 2;
            color_blending.pAttachments = blend_attachments.data();
            break;
        }
        case TransparencyMode::POINTCLOUD:
            // POINTCLOUD only has 1 attachment (the main lit screen)
            color_blend_attachment.blendEnable = vk::False;
            color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blending.attachmentCount = 1;
            color_blending.pAttachments = &color_blend_attachment;
            break;

        case TransparencyMode::OIT_WRITE:
            // OIT_WRITE pass no longer writes to ANY color attachments.
            // It writes to SSBOs/storage images.
            color_blending.attachmentCount = 0; 
            color_blending.pAttachments = nullptr; 
            break;
        
        case TransparencyMode::OIT_COMPOSITE:
            // This is for the final quad, which blends onto the opaque scene
            color_blend_attachment.blendEnable = vk::True;
            color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
            color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Use source alpha
            color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Add to existing
            color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
            
            color_blending.attachmentCount = 1;
            color_blending.pAttachments = &color_blend_attachment;
            break;

    }

    std::vector dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
        vk::DynamicState::eCullMode
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    // --- PUSH CONSTANT SETUP ---
    std::vector<vk::PushConstantRange> push_constant_ranges;
    if(mode == TransparencyMode::OPAQUE || mode == TransparencyMode::OIT_WRITE){
        // Vertex Push Constant (Model Matrix)
        vk::PushConstantRange vert_push_constant_range;
        vert_push_constant_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
        vert_push_constant_range.offset = 0;
        vert_push_constant_range.size = sizeof(glm::mat4);
        push_constant_ranges.push_back(vert_push_constant_range);

        // Fragment Push Constant (Material Data)
        vk::PushConstantRange frag_push_constant_range;
        frag_push_constant_range.stageFlags = vk::ShaderStageFlagBits::eFragment;
        frag_push_constant_range.offset = sizeof(glm::mat4); // Offset it after the vertex data
        frag_push_constant_range.size = sizeof(MaterialPushConstant);
        push_constant_ranges.push_back(frag_push_constant_range);
    }
    // (OIT_COMPOSITE has no push constants)


    vk::PipelineLayoutCreateInfo pipeline_layout_info; 
    std::array<vk::DescriptorSetLayout, 2> oit_write_layouts;
    if(mode == TransparencyMode::OIT_COMPOSITE){
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &*engine.oit_composite_pipeline.descriptor_set_layout;
    }
    else if(mode == TransparencyMode::OIT_WRITE){
        // OIT_WRITE needs Set 0 (Material) and Set 1 (PPLL buffers)
        oit_write_layouts[0] = *(p_info -> descriptor_set_layout); // Material set
        oit_write_layouts[1] = *engine.oit_composite_pipeline.descriptor_set_layout; // PPLL set
        
        pipeline_layout_info.setLayoutCount = 2; // <-- Two sets
        pipeline_layout_info.pSetLayouts = oit_write_layouts.data();
    }
    else{ // OPAQUE
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &*(p_info -> descriptor_set_layout);
    }
    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());; // Use one push constant range
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data(); // Point to our range

    p_info -> layout = vk::raii::PipelineLayout(engine.logical_device, pipeline_layout_info);

    // --- Rendering Info
    vk::Format depth_format = findDepthFormat(engine.physical_device);
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;

    switch(mode){
        case TransparencyMode::OIT_WRITE:
            pipeline_rendering_create_info.colorAttachmentCount = 0;
            pipeline_rendering_create_info.pColorAttachmentFormats = nullptr;
            pipeline_rendering_create_info.depthAttachmentFormat = depth_format;
            break;
        case TransparencyMode::OPAQUE:
            {
                static std::array<vk::Format, 2> color_formats = {
                    engine.swapchain.format,     // Format for Lit
                    vk::Format::eR8G8B8A8Unorm   // Format for Albedo G-Buffer
                };
                pipeline_rendering_create_info.colorAttachmentCount = 2;
                pipeline_rendering_create_info.pColorAttachmentFormats = color_formats.data();
                pipeline_rendering_create_info.depthAttachmentFormat = depth_format;
            }
            break;

        case TransparencyMode::POINTCLOUD:
            pipeline_rendering_create_info.colorAttachmentCount = 1;
            pipeline_rendering_create_info.pColorAttachmentFormats = &engine.swapchain.format;
            pipeline_rendering_create_info.depthAttachmentFormat = depth_format;
            break;

        case TransparencyMode::OIT_COMPOSITE:
            pipeline_rendering_create_info.colorAttachmentCount = 1;
            pipeline_rendering_create_info.pColorAttachmentFormats = &engine.swapchain.format;
            pipeline_rendering_create_info.depthAttachmentFormat = vk::Format::eUndefined; // No depth
            break;
    }

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

vk::raii::Pipeline Pipeline::createShadowPipeline(
    Engine &engine, 
    PipelineInfo *p_info, 
    std::string v_shader, 
    std::string f_shader)
{
    // --- 1. Shaders ---
    vk::raii::ShaderModule vertex_shader_module = createShaderModule(readFile(v_shader), &engine.logical_device);
    vk::raii::ShaderModule frag_shader_module = createShaderModule(readFile(f_shader), &engine.logical_device);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info({}, vk::ShaderStageFlagBits::eVertex, *vertex_shader_module, "main");
    vk::PipelineShaderStageCreateInfo frag_shader_stage_info({}, vk::ShaderStageFlagBits::eFragment, *frag_shader_module, "main");
    vk::PipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    // --- 2. Vertex Input (Position Only) ---
    vk::VertexInputBindingDescription binding_description(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
    // Location 0: Position
    vk::VertexInputAttributeDescription attribute_description(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos));
    vk::PipelineVertexInputStateCreateInfo vertex_input_info({}, 1, &binding_description, 1, &attribute_description);
    
    // --- 3. IA, Viewport, Rasterizer ---
    vk::PipelineInputAssemblyStateCreateInfo input_assembly({}, vk::PrimitiveTopology::eTriangleList);
    
    vk::PipelineViewportStateCreateInfo viewport_state({}, 1, nullptr, 1, nullptr); // Dynamic

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.cullMode = vk::CullModeFlagBits::eFront; // Cull front faces for shadows
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.lineWidth = 1.f;
    // Enable depth bias
    rasterizer.depthBiasEnable = vk::True;
    rasterizer.depthBiasConstantFactor = 1.25f;
    rasterizer.depthBiasSlopeFactor = 1.75f;

    // --- 4. MSAA, Depth, Color ---
    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1); // No MSAA

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.depthTestEnable = vk::True;
    depth_stencil.depthWriteEnable = vk::True;
    depth_stencil.depthCompareOp = vk::CompareOp::eLess;

    // No color attachments
    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.attachmentCount = 0;
    color_blending.pAttachments = nullptr;

    // --- 5. Dynamic State ---
    std::vector dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eCullMode };
    vk::PipelineDynamicStateCreateInfo dynamic_state({}, dynamic_states);

    // --- 6. Pipeline Layout (is already created in createPipelines) ---

    // --- 7. Rendering Info ---
    vk::Format shadow_format = findDepthFormat(engine.physical_device);
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;
    pipeline_rendering_create_info.colorAttachmentCount = 0; // No color
    pipeline_rendering_create_info.depthAttachmentFormat = shadow_format;
    pipeline_rendering_create_info.viewMask = 0b00111111; // Enable multiview for 6 layers

    // --- 8. Create Pipeline ---
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
    pipeline_info.layout = *(p_info->layout);
    pipeline_info.pDepthStencilState = &depth_stencil;

    return vk::raii::Pipeline(engine.logical_device, nullptr, pipeline_info);
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
    push_constant_range.size = sizeof(glm::mat4);

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

vk::raii::Pipeline Pipeline::createComputePipeline(
    Engine& engine, 
    PipelineInfo* p_info, 
    const std::string& c_shader)
{
    // 1. Load Shader
    vk::raii::ShaderModule compute_module = createShaderModule(readFile(c_shader), &engine.logical_device);

    vk::PipelineShaderStageCreateInfo shader_stage_info;
    shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;
    shader_stage_info.module = *compute_module;
    shader_stage_info.pName = "main";

    // 2. Create Pipeline Layout
    // (This assumes descriptor_set_layout has already been created and stored in p_info)
    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &*(p_info->descriptor_set_layout);
    // No push constants for this pipeline
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    p_info->layout = vk::raii::PipelineLayout(engine.logical_device, pipeline_layout_info);

    // 3. Create the Pipeline
    vk::ComputePipelineCreateInfo pipeline_info;
    pipeline_info.stage = shader_stage_info;
    pipeline_info.layout = *(p_info->layout);

    return std::move(vk::raii::Pipeline(engine.logical_device, nullptr, pipeline_info));
}

