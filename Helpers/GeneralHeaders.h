#pragma once

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <stdio.h>
#include <chrono>

#include <vulkan/vulkan_raii.hpp> // this library handles for us the vkCreateXXX
                                  // vkAllocateXXX, vkDestroyXXX, and vkFreeXXX
#include <vulkan/vk_platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


// Taskflow is used for multithreading on the CPU side
#include "taskflow/taskflow.hpp"

// Dynamic Memory Allocation
#include "vk_mem_alloc.h"

#include "stb_image.h"



// Structures used in all the project
struct Vertex{
    glm::vec2 pos;
    glm::vec3 color;

    // Info needed to tell VUlkan how to pass Vertex data to the shader
    static vk::VertexInputBindingDescription getBindingDescription() {
        // First is index, second is stride, last means either read data
        // one vertex at the time, or one instance at the time
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        return{
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
        };
    }
};


struct AllocatedBuffer{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};


struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};
