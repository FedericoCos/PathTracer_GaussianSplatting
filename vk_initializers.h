// Contains helpers to create vulkan structures

#pragma once

#include "vk_types.h"

namespace vkinit {

	// Command pool
	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// Semaphore and Fences
	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags);

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);
}