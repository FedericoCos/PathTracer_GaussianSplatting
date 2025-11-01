#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward declaration

namespace Pipeline{
    vk::raii::Pipeline createGraphicsPipeline(Engine &engine, PipelineInfo *p_info, std::string v_shader, std::string f_shader, TransparencyMode mode, vk::CullModeFlagBits cull_mode);
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device * logical_device);
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(Engine &engine, std::vector<vk::DescriptorSetLayoutBinding> &bindings);

    std::vector<char> readFile(const std::string& filename);
};


