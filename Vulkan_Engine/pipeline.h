#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward declaration

namespace Pipeline{
    vk::raii::Pipeline createGraphicsPipeline(Engine &engine, vk::raii::PipelineLayout &pipeline_layout);
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device * logical_device);
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(Engine &engine);

    std::vector<char> readFile(const std::string& filename);
};


