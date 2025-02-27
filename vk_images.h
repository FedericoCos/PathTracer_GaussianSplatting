// Contains image related vulkan helpers
#pragma once

#include <vulkan/vulkan.h>

namespace vkutil{
    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
}