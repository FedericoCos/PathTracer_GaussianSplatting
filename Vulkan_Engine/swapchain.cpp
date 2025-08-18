#include "swapchain.h"


vk::Format Swapchain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats){
    const auto format_it = std::ranges::find_if(available_formats, 
                [](const auto& format){
                    return format.format == vk::Format::eB8G8R8A8Srgb &&
                            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                });
    return format_it != available_formats.end() ? format_it -> format : available_formats[0].format;
}

vk::Extent2D Swapchain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow * window){
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()){
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

vk::PresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes){
    return std::ranges::any_of(available_present_modes,
        [](const vk::PresentModeKHR value){
            return vk::PresentModeKHR::eMailbox == value;
        }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}


void Swapchain::createSwapChain(vk::raii::PhysicalDevice& physical_device, vk::raii::Device * logical_device, vk::raii::SurfaceKHR& surface, GLFWwindow * window){
    auto surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface);
    swapchain_image_format = chooseSwapSurfaceFormat(physical_device.getSurfaceFormatsKHR(surface));
    swapchain_extent = chooseSwapExtent(surface_capabilities, window);

    auto min_image_count = std::max(3u, surface_capabilities.minImageCount);
    min_image_count = (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount) ?
                        surface_capabilities.maxImageCount : min_image_count;
    
    vk::SwapchainCreateInfoKHR swapchain_create_info;
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = min_image_count;
    swapchain_create_info.imageFormat = swapchain_image_format;
    swapchain_create_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchain_create_info.presentMode = chooseSwapPresentMode(physical_device.getSurfacePresentModesKHR(surface));
    swapchain_create_info.clipped = true;

    swapchain = vk::raii::SwapchainKHR(*logical_device,  swapchain_create_info);
    swapchain_images = swapchain.getImages();
}

void Swapchain::createImageViews(vk::raii::Device * logical_device){
    swapchain_image_views.clear();

    vk::ImageViewCreateInfo imageview_create_info;
    imageview_create_info.viewType = vk::ImageViewType::e2D;
    imageview_create_info.format = swapchain_image_format;
    imageview_create_info.subresourceRange = { 
        vk::ImageAspectFlagBits::eColor, // aspectMask
        0, // baseMipLevel
        1, // levelCount
        0, // baseArrayLayer
        1};// layerCount -> how many images compose the single image (multiple needed for stereographic app)

    for(auto image : swapchain_images){
        imageview_create_info.image = image;
        swapchain_image_views.emplace_back(*logical_device, imageview_create_info);
    }
}

