#pragma once

#include "../Helpers/GeneralHeaders.h"


class Image{
public:
    AllocatedImage createTextureImage(VmaAllocator& vma_allocator, const char * path, vk::raii::Device *logical_device,
                            vk::raii::CommandPool &command_pool, vk::raii::Queue &queue);

    void createImage(uint32_t width, uint32_t height, vk::Format format,vk::ImageTiling tiling, 
        vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedImage& image,
        VmaAllocator& vma_allocator, vk::raii::Device *logical_device);

    void createImageView(AllocatedImage& image, VkImageAspectFlags aspect_flags, vk::raii::Device *logical_device);
    
    vk::raii::Sampler createTextureSampler(vk::raii::PhysicalDevice&, vk::raii::Device*);


    void createDepthResources(vk::raii::PhysicalDevice& physical_device, AllocatedImage& depth_image, uint32_t width,
                            uint32_t height, VmaAllocator& vma_allocator, vk::raii::Device *logical_device);

    void transitionImageLayout(
        const vk::Image& image, 
        vk::ImageLayout old_layout, 
        vk::ImageLayout new_layout,
        vk::raii::CommandPool& command_pool,
        vk::raii::Device* logical_device,
        vk::raii::Queue& queue
    );

private:



};


