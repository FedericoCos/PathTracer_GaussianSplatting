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
    if (mode == TransparencyMode::OIT_COMPOSITE) {
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
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    
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
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil = {};
    if(mode == TransparencyMode::OIT_COMPOSITE){
        depth_stencil.depthTestEnable = vk::False;
        depth_stencil.depthWriteEnable = vk::False;
    } else{
        depth_stencil.depthTestEnable = vk::True;
        depth_stencil.depthWriteEnable = (mode == TransparencyMode::OPAQUE) ? vk::True : vk::False;
        depth_stencil.depthCompareOp = vk::CompareOp::eLess;
        depth_stencil.depthBoundsTestEnable = vk::False;
        depth_stencil.stencilTestEnable = vk::False;
        depth_stencil.depthCompareOp = vk::CompareOp::eLess;
    }

    vk::PipelineColorBlendAttachmentState color_blend_attachment; // For Opawaue/OIT_COMPOSITE
    std::array<vk::PipelineColorBlendAttachmentState, 2> oit_blend_attachments; // For OIT_WRITE

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = vk::False;

    switch(mode){
        case TransparencyMode::OPAQUE:
            color_blend_attachment.blendEnable = vk::False;
            color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blending.attachmentCount = 1;
            color_blending.pAttachments = &color_blend_attachment;
            break;

        case TransparencyMode::OIT_WRITE:
            // Attachment 0: Accumulation (Additive)
            oit_blend_attachments[0].blendEnable = vk::True;
            oit_blend_attachments[0].colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            oit_blend_attachments[0].srcColorBlendFactor = vk::BlendFactor::eOne;
            oit_blend_attachments[0].dstColorBlendFactor = vk::BlendFactor::eOne;
            oit_blend_attachments[0].colorBlendOp = vk::BlendOp::eAdd;
            oit_blend_attachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne; 
            oit_blend_attachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOne;
            oit_blend_attachments[0].alphaBlendOp = vk::BlendOp::eAdd;

            // Attachment 1: Revealage (Multiplicative)
            // Result = Src * Dst + Dst * 0 = (1 - alpha) * OldReveal
            oit_blend_attachments[1].blendEnable = vk::True;
            oit_blend_attachments[1].colorWriteMask = vk::ColorComponentFlagBits::eR; // Only need one channel
            oit_blend_attachments[1].srcColorBlendFactor = vk::BlendFactor::eDstColor;
            oit_blend_attachments[1].dstColorBlendFactor = vk::BlendFactor::eZero;
            oit_blend_attachments[1].colorBlendOp = vk::BlendOp::eAdd;
            oit_blend_attachments[1].srcAlphaBlendFactor = vk::BlendFactor::eZero; // (Not really used)
            oit_blend_attachments[1].dstAlphaBlendFactor = vk::BlendFactor::eZero;
            oit_blend_attachments[1].alphaBlendOp = vk::BlendOp::eAdd;

            color_blending.attachmentCount = 2;
            color_blending.pAttachments = oit_blend_attachments.data();
            break;
        
        case TransparencyMode::OIT_COMPOSITE:
            color_blend_attachment.blendEnable = vk::True;
            color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
            color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
            color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
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
    if(mode != TransparencyMode::OIT_COMPOSITE){
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
    if(mode == TransparencyMode::OIT_COMPOSITE){
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &*engine.oit_composite_pipeline.descriptor_set_layout;
    }
    else{
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &*(p_info -> descriptor_set_layout);
    }
    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());; // Use one push constant range
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data(); // Point to our range

    p_info -> layout = vk::raii::PipelineLayout(engine.logical_device, pipeline_layout_info);

    // --- Rendering Info
    vk::Format depth_format = findDepthFormat(engine.physical_device);
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;
    
    // Setup attachment formats based on pass
    std::array<vk::Format, 2> oit_write_formats = { 
        static_cast<vk::Format>(engine.oit_accum_image.image_format), 
        static_cast<vk::Format>(engine.oit_reveal_image.image_format) 
    };

    switch(mode){
        case TransparencyMode::OIT_WRITE:
            pipeline_rendering_create_info.colorAttachmentCount = 2;
            pipeline_rendering_create_info.pColorAttachmentFormats = oit_write_formats.data();
            pipeline_rendering_create_info.depthAttachmentFormat = depth_format;
            break;
        case TransparencyMode::OPAQUE:
        case TransparencyMode::OIT_COMPOSITE:
            pipeline_rendering_create_info.colorAttachmentCount = 1;
            pipeline_rendering_create_info.pColorAttachmentFormats = &engine.swapchain.format;
            // OPAQUE has depth, COMPOSITE does not
            pipeline_rendering_create_info.depthAttachmentFormat = (mode == TransparencyMode::OPAQUE) ? depth_format : vk::Format::eUndefined;
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

