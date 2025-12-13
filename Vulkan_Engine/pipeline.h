#pragma once

#include "../Helpers/GeneralHeaders.h"

namespace Pipeline{
    vk::raii::Pipeline createGraphicsPipeline(PipelineInfo *p_info, std::string v_shader, std::string f_shader, TransparencyMode mode, vk::CullModeFlagBits cull_mode,
                                            vk::raii::Device &logical_device, vk::raii::PhysicalDevice &physical_device, vk::Format &swapchain_format);
    
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device& logical_device);
    
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(std::vector<vk::DescriptorSetLayoutBinding> &bindings, vk::raii::Device &logical_device);
    vk::raii::DescriptorSetLayout createDescriptorSetLayout(std::vector<vk::DescriptorSetLayoutBinding> &bindings, vk::raii::Device& logical_device, vk::DescriptorSetLayoutCreateInfo& layout_info);

    std::vector<char> readFile(const std::string& filename);

    // Removed createShadowPipeline (Raster shadows are deleted)

    vk::raii::Pipeline createRayTracingPipeline(
        PipelineInfo* p_info, 
        vk::raii::Device &logical_device, 
        const std::string& rgen_torus,
        const std::string& rgen_camera,
        const std::string& rmiss_primary,
        const std::string& rmiss_shadow,
        const std::string& rchit,
        const std::string& rahit,
        uint32_t push_constant_size,
        PFN_vkCreateRayTracingPipelinesKHR &vkCreateRayTracingPipelinesKHR
    );
};