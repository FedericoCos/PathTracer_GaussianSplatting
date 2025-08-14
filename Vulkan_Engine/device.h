#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"


class Device{
public:
    vk::raii::PhysicalDevice& getPhysicalDevice(vk::raii::Instance& instance){
        if(physical_device == nullptr){
            pickPhysicalDevice(instance);
        }
        return physical_device;
    }

    vk::raii::Device * getLogicalDevice(){
        if(logical_device == nullptr){
            createLogicalDevice();
        }

        return &logical_device;
    }

    vk::raii::Queue& getGraphicsQueue(){
        if(graphics_queue == nullptr){
            createLogicalDevice();
        }

        return graphics_queue;
    }

    uint32_t findQueueFamilies();


private:
    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device logical_device = nullptr;
    vk::raii::Queue graphics_queue = nullptr;

    std::vector<const char*> device_extensions = {
        vk::KHRSwapchainExtensionName, // extension required for presenting rendered images to the window
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };

    bool isDeviceSuitable(vk::raii::PhysicalDevice& device);
    void pickPhysicalDevice(vk::raii::Instance& instance);
    void createLogicalDevice();
};

