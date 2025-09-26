#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

#include "../Helpers/GLFWhelper.h"

class Engine;



namespace Swapchain{
    vk::raii::SwapchainKHR createSwapChain(Engine &engine, vk::Format &swapchain_image_format, vk::Extent2D &swapchain_extent);
    std::vector<vk::raii::ImageView> createImageViews(Engine &engine, const vk::Format &swapchain_image_format, const std::vector<vk::Image> &swapchain_images);

    vk::Format chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow * window);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &available_present_modes);
};