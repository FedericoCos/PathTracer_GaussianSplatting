#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward Declaration


namespace Image{
    AllocatedImage createTextureImage(Engine &engine, const char * path);

    AllocatedImage createImage(uint32_t width, uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits num_samples, vk::Format format,vk::ImageTiling tiling, 
        vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
        Engine &engine);

    vk::raii::ImageView createImageView(AllocatedImage& image, Engine &engine);
    
    vk::raii::Sampler createTextureSampler(vk::raii::PhysicalDevice&, vk::raii::Device*, uint32_t);


    void createDepthResources(vk::raii::PhysicalDevice& physical_device, AllocatedImage& depth_image, uint32_t width,
                            uint32_t height, Engine &engine);

    void transitionImageLayout(
        const vk::Image& image,
        uint32_t mip_levels, 
        vk::ImageLayout old_layout, 
        vk::ImageLayout new_layout,
        Engine &engine
    );
    void generateMipmaps(AllocatedImage &image, Engine &engine);
};
