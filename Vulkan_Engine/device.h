#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

namespace Device{
    vk::raii::PhysicalDevice pickPhysicalDevice(const vk::raii::Instance &instance);
    vk::raii::Device createLogicalDevice(const vk::raii::PhysicalDevice &physical_device, const vk::raii::SurfaceKHR &surface, QueueFamilyIndices &indices);
    vk::raii::Queue getQueue(const vk::raii::Device &logical_device, const uint32_t &indices);

    bool isDeviceSuitable(const vk::raii::PhysicalDevice& device, bool descrete);
    QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice &physical_device, const vk::raii::SurfaceKHR &surface);
};

