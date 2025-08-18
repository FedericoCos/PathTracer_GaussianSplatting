#pragma once

#include "../Helpers/GeneralHeaders.h"


class Pipeline{
public:
    void init(vk::raii::Device *logical_device, vk::Format& swapchain_image_format){
        if(instantiated){
            return;
        }

        createGraphicsPipeline(logical_device, swapchain_image_format);
        instantiated = true;
    }


    vk::raii::PipelineLayout * getGraphicsPipelineLayout(){
        if(!instantiated){
            throw std::runtime_error("Graphics pipeline not instantiated");
        }

        return &graphics_pipeline_layout;
    }

    vk::raii::Pipeline * getGraphicsPipeline(){
        if(!instantiated){
            throw std::runtime_error("Graphics pipeline not instantiated");
        }

        return &graphics_pipeline;
    }



private:
    vk::raii::PipelineLayout graphics_pipeline_layout = nullptr;
    vk::raii::Pipeline graphics_pipeline = nullptr;

    bool instantiated = false;

    void createGraphicsPipeline(vk::raii::Device * logical_device, vk::Format& swapchain_image_format);
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code, vk::raii::Device * logical_device) const;

    std::vector<char> readFile(const std::string& filename);
};


