// Contains image related vulkan helpers
#include "vk_images.h"

#include "vk_initializers.h"

void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageBarrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // TODO!! It stalls the GPU pipeline a bit. If many transitions per frame are needed as part
                                                                      // of a post process chain, you want to avoid doing this, and instead use stagemasks more accurate to what is needed
                                                                      // The barrier will stop the gpucommands completely when it arrives at the barrier
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT; // Also access mask indentifies how the GPU operations will be handled when barrier is met
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    /**
     * As part of the barrier, we need to use a VkImageSubresourceRange too. This lets us target a part of the image with the barrier. 
     * Its most useful for things like array images or mipmapped images, where we would only need to barrier on a given layer 
     * or mipmap level. We are going to completely default it and have it transition all mipmap levels and layers.
     */
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo depInfo {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    /**
     * Once we have the range and the barrier, we pack them into a VkDependencyInfo struct and call VkCmdPipelineBarrier2. 
     * It is possible to layout transitions multiple images at once by sending more imageMemoryBarriers into the dependency info, 
     * which is likely to improve performance if we are doing transitions or barriers for multiple things at once.
     */
    vkCmdPipelineBarrier2(cmd, &depInfo);
}