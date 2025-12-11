#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

#include "../Helpers/GLFWhelper.h"



namespace Swapchain{
    SwapChainBundle createSwapChain(const vk::raii::PhysicalDevice &physical_device, const vk::raii::Device &logical_device,
                                    const vk::raii::SurfaceKHR &surface, const QueueFamilyIndices &queue_indices, 
                                    const int &win_width, const int &win_height);

    vk::Format chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const int &win_width, const int &win_height);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &available_present_modes);
};