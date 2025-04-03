// To handle images like textures, ...

#include "vk_images.h"
#include "vk_engine.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

AllocatedImage vkimage::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VulkanEngine& engine){
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped){
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vmaCreateImage(engine.getAllocator(), &img_info, &allocInfo, &newImage.image, &newImage.allocation, nullptr);

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT){
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;
    vkCreateImageView(engine.getDevice(), &view_info, nullptr, &newImage.imageView);

    return newImage;
}


AllocatedImage vkimage::create_image(void * data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VulkanEngine& engine){
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = engine.create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadBuffer.info.pMappedData, data, data_size);
    AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped, engine);

    engine.immediate_submit([&](VkCommandBuffer cmd) {
		transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 1);
		});

	engine.destroy_buffer(uploadBuffer);

	return new_image;
}

void vkimage::destroy_image(const AllocatedImage& img, VulkanEngine& engine){
    vkDestroyImageView(engine.getDevice(), img.imageView, nullptr);
    vmaDestroyImage(engine.getAllocator(), img.image, img.allocation);
}

VkImageSubresourceRange vkimage::image_subresource_range(VkImageAspectFlags aspectMask){
    VkImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return subImage;
}


void vkimage::transition_image(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    uint32_t mipLevels,
    uint32_t layerCount,
    VkPipelineStageFlags2 srcStage,
    VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStage,
    VkAccessFlags2 dstAccess)
{
    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = image;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    // Automatically detect aspect mask
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    // If caller didn't specify stages, pick defaults based on layouts
    if (srcStage == 0 || dstStage == 0)
    {
        switch (oldLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            srcAccess = 0;
            break;
        case VK_IMAGE_LAYOUT_GENERAL:
            srcStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            srcAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            break;
        default:
            srcStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            srcAccess = VK_ACCESS_2_MEMORY_WRITE_BIT;
            break;
        }

        switch (newLayout)
        {
        case VK_IMAGE_LAYOUT_GENERAL:
            dstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            dstAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            dstAccess = 0;
            break;
        default:
            dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            break;
        }
    }

    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;

    VkDependencyInfo depInfo = {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void vkimage::copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize){
    VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = 1;

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo); // TODO, BlitImage is slow
}

std::optional<AllocatedImage> vkimage::load_image(VulkanEngine& engine, fastgltf::Asset& asset, fastgltf::Image& image){
    AllocatedImage newImage {};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0);
                assert(filePath.uri.isLocalPath());

                const std::string path(filePath.uri.path().begin(),
                    filePath.uri.path().end());

                unsigned char * data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if(data){
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    newImage = create_image(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false, engine);
                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char * data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);

                if(data){
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    newImage = create_image(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false, engine);
                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                               // specify LoadExternalBuffers, meaning all buffers
                                               // are already loaded into a vector.
                               [](auto& arg) {},
                               [&](fastgltf::sources::Vector& vector) {
                                   unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,false, engine);

                                       stbi_image_free(data);
                                   }
                               } },
                    buffer.data);
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    } else {
        return newImage;
    }
}


