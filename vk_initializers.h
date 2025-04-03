// Code for creation of Vulkan Structures
#pragma once

#include "vk_types.h"

namespace vkinit{
    /**
     * Allows to initialize an image info for image allocation
     * Sets mipmap levels to 1 and MSAA to 1
     */
    VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

    /**
     * Allows to create an image view for an image
     */
    VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

    VkCommandPoolCreateInfo command_pool_create_info(uint32_t queuefamilyIndex, VkCommandPoolCreateFlags flags = 0);
    VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
    VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);
    VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

    VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags aspectMask);
    VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
    VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                            VkSemaphoreSubmitInfo* waitSemaphoreInfo);


    VkPipelineLayoutCreateInfo pipeline_layout_create_info();
    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits shaderStageFlag, VkShaderModule module);

    VkRenderingAttachmentInfo attachment_info(VkImageView view, VkClearValue* clear ,VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);
	VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
		VkRenderingAttachmentInfo* depthAttachment);
    VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout);
}