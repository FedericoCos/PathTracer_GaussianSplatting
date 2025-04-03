// To handle images like textures, ...
#pragma once

#include "vk_types.h"

class VulkanEngine; // forward declaration

namespace vkimage{
    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VulkanEngine& engine);
    /**
     * Creates an AllocateImage object to later use as resource (such as texture)
     */
    AllocatedImage create_image(void * data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VulkanEngine& engine);
    void destroy_image(const AllocatedImage& img, VulkanEngine& engine);

    /**
     * Allows to consider only specific subparts of an image
     */
    VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

    /**
     * Allows for transitioning the layout of an image
     */
    void transition_image(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels,
        uint32_t layerCount,
        VkPipelineStageFlags2 srcStage = 0,
        VkAccessFlags2 srcAccess = 0,
        VkPipelineStageFlags2 dstStage = 0,
        VkAccessFlags2 dstAccess = 0);

    
    void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

    std::optional<AllocatedImage> load_image(VulkanEngine& engine, fastgltf::Asset& asset, fastgltf::Image& image);

} 