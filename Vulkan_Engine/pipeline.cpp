#include "pipeline.h"
#include "engine.h"


vk::raii::Pipeline Pipeline::createGraphicsPipeline(Engine &engine, vk::raii::PipelineLayout &pipeline_layout){
    vk::raii::ShaderModule vertex_shader_module = createShaderModule(readFile("shaders/basic/vertex.spv"), &engine.logical_device_bll);
    vk::raii::ShaderModule frag_shader_module = createShaderModule(readFile("shaders/basic/fragment.spv"), &engine.logical_device_bll);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info;
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_shader_stage_info.module = *vertex_shader_module;
    vert_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_shader_stage_info;
    frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_shader_stage_info.module = *frag_shader_module;
    frag_shader_stage_info.pName = "main";

    // Shader stage array
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info, frag_shader_stage_info
    };


    auto binding_description = Vertex::getBindingDescription();
    auto attribute_descriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input_info; // Describes the fromat of the vartex data
                                                              // that will be passed to the vertex shader
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = attribute_descriptions.size();
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();
    
    vk::PipelineInputAssemblyStateCreateInfo input_assembly; // describes what kind of geometry will be drawn
                                                             // and if primitive restart is enabled
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList; // traingle from every 3 vertices w\out reuse
    
    vk::PipelineViewportStateCreateInfo viewport_state; // Describes the region of the framebuffer that the output will render to
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    // Vieport define the transformation from the image to the framebuffer
    // Scissor rectangles define in which region pixels will be stored

    // Rasterizer -> creates fragments, performs depth testing, face culling, scissor test
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = vk::False; // Set to true for shadow mapping
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = vk::False,
    rasterizer.depthBiasSlopeFactor = 1.f;
    rasterizer.lineWidth = 1.f; // Any thickness larger than 1.f requires to enable the widelines GPU feature

    // Important for antialiasing
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e8; // TO FIX!!!!!
    multisampling.sampleShadingEnable = vk::False; // Disabled for now

    // Depth stencil information, for depth ordering
    vk::PipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.depthTestEnable = vk::True;
    depth_stencil.depthWriteEnable = vk::True;
    depth_stencil.depthCompareOp = vk::CompareOp::eLess;
    depth_stencil.depthBoundsTestEnable = vk::False;
    depth_stencil.stencilTestEnable = vk::False;

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = vk::False;
    color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    
    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = vk::False;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;


    std::vector dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    vk::PipelineLayoutCreateInfo pipeline_layout_info; 
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &*engine.descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    pipeline_layout = vk::raii::PipelineLayout(engine.logical_device_bll, pipeline_layout_info);

    vk::Format depth_format = findDepthFormat(engine.physical_device);

    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    pipeline_rendering_create_info.pColorAttachmentFormats = &engine.swapchain.format;
    pipeline_rendering_create_info.depthAttachmentFormat = depth_format;

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
    pipeline_info.layout = pipeline_layout;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.renderPass = nullptr; // Since I am using dynamic rendering instead of a traditional render pass

    return std::move(vk::raii::Pipeline(engine.logical_device_bll, nullptr, pipeline_info));
}

vk::raii::ShaderModule Pipeline::createShaderModule(const std::vector<char>& code, vk::raii::Device * logical_device){
    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = code.size() * sizeof(char);
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vk::raii::ShaderModule shader_module{ *logical_device, create_info };
    return shader_module;
}

vk::raii::DescriptorSetLayout Pipeline::createDescriptorSetLayout(Engine &engine)
{
    std::array bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, // position and type in buffer 
            1, // Decriptor count -> Can be array
            vk::ShaderStageFlagBits::eVertex, nullptr),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
    };
    vk::DescriptorSetLayoutCreateInfo layout_info({}, bindings.size(), bindings.data());

    return std::move(vk::raii::DescriptorSetLayout(engine.logical_device_bll, layout_info));
}

std::vector<char> Pipeline::readFile(const std::string& filename){
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open()){
        throw std::runtime_error("failed to open file: " + filename);
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}



