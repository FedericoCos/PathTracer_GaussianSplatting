#include "image.h"
#include "engine.h"

AllocatedImage Image::createTextureImage(Engine &engine, const char * path, vk::Format format)
{
    int tex_width,tex_height, tex_channels;

    stbi_uc* pixels = stbi_load(path, &tex_width, &tex_height,&tex_channels, STBI_rgb_alpha);
    vk::DeviceSize image_size = tex_width * tex_height * 4;

    if(!pixels){
        throw std::runtime_error("failed to load texture image");
    }

    AllocatedBuffer staging_buffer;
    createBuffer(engine.vma_allocator, image_size,
                vk::BufferUsageFlagBits::eTransferSrc, 
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);
    
    void *data;
    vmaMapMemory(engine.vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, pixels, image_size);

    stbi_image_free(pixels);

    uint32_t mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(tex_width, tex_height)))) + 1;
    AllocatedImage texture_image = createImage(tex_width, tex_height, mip_levels, vk::SampleCountFlagBits::e1, format, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal, engine);
    
    texture_image.image_view = createImageView(texture_image, engine);

    transitionImageLayout(texture_image.image, texture_image.mip_levels, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, engine);
    copyBufferToImage(staging_buffer.buffer, texture_image.image, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height), &engine.logical_device, engine.command_pool_transfer, engine.transfer_queue);
    generateMipmaps(texture_image, engine);

    vmaUnmapMemory(engine.vma_allocator, staging_buffer.allocation);

    return texture_image;
}

AllocatedImage Image::createImage(uint32_t width, uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits num_samples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, Engine &engine)
{
    AllocatedImage image;
    image.image_format = static_cast<VkFormat>(format);
    image.image_extent = vk::Extent3D(width, height, 1);
    image.mip_levels = mip_levels;

    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.format = format;
    image_info.extent = image.image_extent;
    image_info.arrayLayers = 1;
    image_info.tiling = tiling;
    image_info.usage = usage;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.mipLevels = image.mip_levels;
    image_info.samples = num_samples;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if(engine.vma_allocator == nullptr){
        std::cout << "Error\n";
    }
    vmaCreateImage(engine.vma_allocator, reinterpret_cast<const VkImageCreateInfo*>(&image_info), &alloc_info, &image.image, &image.allocation, nullptr);

    return image;
}

vk::raii::ImageView Image::createImageView(AllocatedImage &image, Engine &engine)
{
    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(image.image_format == VK_FORMAT_D32_SFLOAT){
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

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

    return std::move(vk::raii::ImageView(engine.logical_device, view_info));
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
    sampl.mipLodBias = 0.0f;
    sampl.anisotropyEnable = vk::True;
    sampl.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    sampl.compareEnable = vk::False;
    sampl.compareOp = vk::CompareOp::eAlways;
    sampl.minLod = 0.0f;
    sampl.maxLod = static_cast<float>(mip_levels);

    return vk::raii::Sampler(*logical_device, sampl);

}

void Image::createDepthResources(vk::raii::PhysicalDevice& physical_device, 
                                AllocatedImage& depth_image, 
                                uint32_t width,
                                uint32_t height,
                                Engine &engine)
{
    vk::Format depthFormat = findDepthFormat(physical_device);

    depth_image = createImage(width, height, 1, engine.mssa_samples, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, 
                vk::MemoryPropertyFlagBits::eDeviceLocal, engine);

    depth_image.image_view = createImageView(depth_image, engine);

}

void Image::transitionImageLayout(const vk::Image &image, uint32_t mip_levels, vk::ImageLayout old_layout, vk::ImageLayout new_layout, Engine &engine)
{
    bool is_graphic_transition = (new_layout == vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::raii::CommandPool& pool = is_graphic_transition ? engine.command_pool_graphics : engine.command_pool_transfer;
    vk::raii::Queue& queue = is_graphic_transition ? engine.graphics_queue : engine.transfer_queue;

    auto command_buffer = beginSingleTimeCommands(pool, &engine.logical_device);

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

void Image::generateMipmaps(AllocatedImage &image, Engine &engine)
{
    vk::FormatProperties format_properties = engine.physical_device.getFormatProperties(static_cast<vk::Format>(image.image_format));
    if(!(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)){
        throw std::runtime_error("texture image format does not support linear blitting!");
    }


    vk::raii::CommandBuffer command_buffer = beginSingleTimeCommands(engine.command_pool_graphics, &engine.logical_device);
    vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
                                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                                vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, image.image);
    barrier.subresourceRange.aspectMask = static_cast<vk::ImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT);
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = image.image_extent.width;
    int32_t mip_height = image.image_extent.height;

    for(uint32_t i = 1; i < image.mip_levels; i++){
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dst_offsets;
        offsets[0] = vk::Offset3D(0, 0, 0);
        offsets[1] = vk::Offset3D(mip_width, mip_height, 1);
        dst_offsets[0] = vk::Offset3D(0, 0, 0);
        dst_offsets[1] = vk::Offset3D(mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1);
        vk::ImageBlit blit = {};
        blit.srcOffsets = offsets;
        blit.dstOffsets = dst_offsets;
        blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
        blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);

        command_buffer.blitImage(image.image, vk::ImageLayout::eTransferSrcOptimal, image.image, vk::ImageLayout::eTransferDstOptimal, { blit }, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if(mip_width > 1) mip_width /= 2;
        if(mip_height > 1) mip_height /= 2;
    }

    barrier.subresourceRange.baseMipLevel = image.mip_levels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

    endSingleTimeCommands(command_buffer, engine.graphics_queue);
}
