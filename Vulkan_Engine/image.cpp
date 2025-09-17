#include "image.h"

AllocatedImage Image::createTextureImage(VmaAllocator& vma_allocator, const char * path, vk::raii::Device *logical_device, vk::raii::CommandPool &command_pool, vk::raii::Queue &queue)
{
    int tex_width,tex_height, tex_channels;

    stbi_uc* pixels = stbi_load(path, &tex_width, &tex_height,&tex_channels, STBI_rgb_alpha);
    vk::DeviceSize image_size = tex_width * tex_height * 4;

    if(!pixels){
        throw std::runtime_error("failed to load texture image");
    }

    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, image_size,
                vk::BufferUsageFlagBits::eTransferSrc, 
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);
    
    void *data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, pixels, image_size);

    stbi_image_free(pixels);

    AllocatedImage texture_image;
    texture_image.mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(tex_width, tex_height)))) + 1;
    createImage(tex_width, tex_height, texture_image.mip_levels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal, texture_image, vma_allocator, logical_device);
    
    transitionImageLayout(texture_image.image, texture_image.mip_levels, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, command_pool, logical_device, queue);
    copyBufferToImage(staging_buffer.buffer, texture_image.image, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height), logical_device, command_pool, queue);
    // transitionImageLayout(texture_image.image, texture_image.mip_levels, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, command_pool, logical_device, queue);
    /**
     * Each level will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 
     * after the blit command reading from it is finished.
     */

    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);

    return texture_image;
}

void Image::createImage(uint32_t width, uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits num_samples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedImage &image, VmaAllocator &vma_allocator, vk::raii::Device *logical_device)
{
    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = format;
    image_info.extent = vk::Extent3D{width, height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.tiling = tiling;
    image_info.usage = usage;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.mipLevels = mip_levels;
    image_info.samples = num_samples;

    image.image_format = static_cast<VkFormat>(format);
    image.image_extent = vk::Extent3D{width, height, 1};

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if(vma_allocator == nullptr){
        std::cout << "Error\n";
    }
    vmaCreateImage(vma_allocator, reinterpret_cast<const VkImageCreateInfo*>(&image_info), &alloc_info, &image.image, &image.allocation, nullptr);

    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(image.image_format == VK_FORMAT_D32_SFLOAT){
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    createImageView(image, aspect_flags, logical_device);
}

void Image::createImageView(AllocatedImage &image, VkImageAspectFlags aspect_flags, vk::raii::Device *logical_device)
{
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext = nullptr;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.image = image.image;
    view_info.format = image.image_format;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = image.mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.subresourceRange.aspectMask = aspect_flags;

    std::cout << image.mip_levels << std::endl;

    vkCreateImageView(**logical_device, &view_info, nullptr, &image.image_view);

}

vk::raii::Sampler Image::createTextureSampler(vk::raii::PhysicalDevice& physical_device, vk::raii::Device *logical_device, uint32_t mip_levels)
{
    vk::PhysicalDeviceProperties properties = physical_device.getProperties();
    vk::SamplerCreateInfo sampl = {};
    sampl.magFilter = vk::Filter::eLinear;
    sampl.minFilter = vk::Filter::eLinear;
    sampl.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampl.addressModeU = vk::SamplerAddressMode::eRepeat;
    sampl.addressModeV = vk::SamplerAddressMode::eRepeat;
    sampl.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampl.mipLodBias = 1;
    sampl.anisotropyEnable = vk::True;
    sampl.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    sampl.compareEnable = vk::False;
    sampl.compareOp = vk::CompareOp::eAlways;
    sampl.minLod = 1;
    sampl.maxLod = static_cast<float>(mip_levels);

    return vk::raii::Sampler(*logical_device, sampl);

}

void Image::createDepthResources(vk::raii::PhysicalDevice& physical_device, 
                                AllocatedImage& depth_image, 
                                uint32_t width,
                                uint32_t height,
                                VmaAllocator& vma_allocator, vk::raii::Device *logical_device)
{
    vk::Format depthFormat = findDepthFormat(physical_device);

    depth_image.mip_levels = 1;
    createImage(width, height, 1, vk::SampleCountFlagBits::e8, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, depth_image, vma_allocator, logical_device); // TO FIX HERE WITH MSAA

}

void Image::transitionImageLayout(const vk::Image &image, uint32_t mip_levels, vk::ImageLayout old_layout, vk::ImageLayout new_layout, vk::raii::CommandPool &command_pool, vk::raii::Device *logical_device, vk::raii::Queue &queue)
{
    auto command_buffer = beginSingleTimeCommands(command_pool, logical_device);

    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.image = image;
    barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    barrier.subresourceRange.levelCount = mip_levels;

    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if(old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal){
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if(old_layout == vk::ImageLayout::eTransferDstOptimal && 
        new_layout == vk::ImageLayout::eShaderReadOnlyOptimal){
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else{
        throw std::invalid_argument("unsupported layout transition!");
    }

    command_buffer.pipelineBarrier(source_stage, destination_stage, {}, {}, nullptr, barrier);

    endSingleTimeCommands(command_buffer, queue);
}
