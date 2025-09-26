#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

#include "../Helpers/GLFWhelper.h"

class Engine;



namespace Swapchain{
    SwapChainBundle createSwapChain(Engine &engine);

    vk::Format chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow * window);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &available_present_modes);
};