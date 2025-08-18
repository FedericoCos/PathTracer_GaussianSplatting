#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

#include "../Helpers/GLFWhelper.h"



class Swapchain{
public:

    void init(vk::raii::PhysicalDevice& physical_device, vk::raii::Device * logical_device,vk::raii::SurfaceKHR& surface, GLFWwindow * window){
        if(instantiated){
            return;
        }
        createSwapChain(physical_device, logical_device, surface, window);
        createImageViews(logical_device);
        instantiated = true;
    }

    vk::raii::SwapchainKHR * getSwapchain(){
        if(!instantiated){
            throw std::runtime_error("Swapchain has not been instantiated");
        }

        return &swapchain;
    }

    std::vector<vk::Image>& getSwapchainImages(){
        if(!instantiated){
            throw std::runtime_error("Swapchain has not been instantiated");
        }

        return swapchain_images;
    }

    vk::Extent2D& getSwapchainExtent(){
        if(!instantiated){
            throw std::runtime_error("Swapchain has not been instantiated");
        }

        return swapchain_extent;
    }

    vk::Format& getSwapchainImageFormat(){
        if(!instantiated){
            throw std::runtime_error("Swapchain has not been instantiated");
        }

        return swapchain_image_format;
    }

    std::vector<vk::raii::ImageView>& getSwapchainImageViews(){
        if(!instantiated){
            throw std::runtime_error("Swapchain has not been instantiated");
        }

        return swapchain_image_views;
    }



private:
    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> swapchain_images;
    vk::Format swapchain_image_format = vk::Format::eUndefined;
    vk::Extent2D swapchain_extent;
    std::vector<vk::raii::ImageView> swapchain_image_views;

    bool instantiated = false;


    void createSwapChain(vk::raii::PhysicalDevice& physical_device, vk::raii::Device * logical_device,vk::raii::SurfaceKHR& surface, GLFWwindow * window);
    void createImageViews(vk::raii::Device * logical_device);

    vk::Format chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow * window);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &available_present_modes);
};