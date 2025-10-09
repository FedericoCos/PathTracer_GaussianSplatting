#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward declaration

namespace Pipeline{
    vk::raii::Pipeline createGraphicsPipeline(Engine &engine, vk::raii::PipelineLayout &pipeline_layout, std::string v_shader, std::string f_shader, bool is_transparent, vk::CullModeFlagBits cull_mode);
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device * logical_device);
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(Engine &engine);

    std::vector<char> readFile(const std::string& filename);
};


