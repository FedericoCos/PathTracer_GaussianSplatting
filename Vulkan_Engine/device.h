#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"


class Device{
public:
    void init(vk::raii::Instance& instance, vk::raii::SurfaceKHR& surface){
        if(instantiated){
            return;
        }
        pickPhysicalDevice(instance);
        createLogicalDevice(surface);
        instantiated = true;

    }

    vk::raii::PhysicalDevice& getPhysicalDevice(){
        if(!instantiated){
            throw std::runtime_error("Device class has not been instantiated");
        }
        return physical_device;
    }

    vk::raii::Device * getLogicalDevice(){
        if(!instantiated){
            throw std::runtime_error("Device class has not been instantiated");
        }
        return &logical_device;
    }

    vk::raii::Queue& getGraphicsQueue(){
        if(!instantiated){
            throw std::runtime_error("Device class has not been instantiated");
        }
        return graphics_presentation_queue;
    }

    uint32_t& getGraphicsIndex(){
        if(!instantiated){
            throw std::runtime_error("Device class has not been instantiated");
        }
        return graphics_index;
    }


private:
    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device logical_device = nullptr;
    vk::raii::Queue graphics_presentation_queue = nullptr;
    uint32_t graphics_index;

    bool instantiated = false;

    std::vector<const char*> device_extensions = {
        vk::KHRSwapchainExtensionName, // extension required for presenting rendered images to the window
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };

    bool isDeviceSuitable(vk::raii::PhysicalDevice& device, bool descrete);
    void pickPhysicalDevice(vk::raii::Instance& instance);
    void createLogicalDevice(vk::raii::SurfaceKHR& surface);
    uint32_t findQueueFamilies(vk::raii::SurfaceKHR& surface);
};

