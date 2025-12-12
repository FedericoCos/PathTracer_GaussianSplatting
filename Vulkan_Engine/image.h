#pragma once

#include "../Helpers/GeneralHeaders.h"


namespace Image{
    AllocatedImage createTextureImage(const char * path, vk::Format format, vk::raii::PhysicalDevice &physical_device, 
                                    vk::raii::Device &logical_device, PoolQueue &pool_and_queue, VmaAllocator &vma_allocator);

    AllocatedImage createImage(uint32_t width, uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits num_samples, vk::Format format,vk::ImageTiling tiling, 
        vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
        VmaAllocator &vma_allocator);

    vk::raii::ImageView createImageView(AllocatedImage& image, vk::raii::Device &logical_device);
    
    vk::raii::Sampler createTextureSampler(vk::raii::PhysicalDevice&, vk::raii::Device*, uint32_t);


    void createDepthResources(vk::raii::PhysicalDevice& physical_device, vk::raii::Device &logical_device, AllocatedImage& depth_image, uint32_t width,
                            uint32_t height, VmaAllocator &vma_allocator);

    void transitionImageLayout(
        const vk::Image& image,
        uint32_t mip_levels, 
        vk::ImageLayout old_layout, 
        vk::ImageLayout new_layout,
        PoolQueue &pool_and_queue,
        vk::raii::Device &logical_device
    );
    void generateMipmaps(AllocatedImage &image, const vk::raii::PhysicalDevice &physical_device, vk::raii::Device &logical_device, 
                        const vk::raii::CommandPool &command_pool_graphics, const vk::raii::Queue &graphics_queue);
    void resolveImage(vk::raii::CommandBuffer &cmd, const AllocatedImage &src_image, const AllocatedImage &dst_image, const vk::Extent2D &extent);
};
