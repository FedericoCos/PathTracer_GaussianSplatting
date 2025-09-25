#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward Declaration

namespace Device{
    vk::raii::PhysicalDevice pickPhysicalDevice(const Engine &engine);
    vk::raii::Device createLogicalDevice(const Engine &engine, QueueFamilyIndices &indices);
    vk::raii::Queue getQueue(const Engine &engine, const uint32_t &indices);

    bool isDeviceSuitable(const vk::raii::PhysicalDevice& device, bool descrete);
    QueueFamilyIndices findQueueFamilies(const Engine& engine);
};

