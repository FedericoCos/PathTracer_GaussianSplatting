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
#include <array>
#include <unordered_map>
#include <optional>

#include <vulkan/vulkan_raii.hpp> // this library handles for us the vkCreateXXX
                                  // vkAllocateXXX, vkDestroyXXX, and vkFreeXXX
#include <vulkan/vk_platform.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


// Taskflow is used for multithreading on the CPU side
#include "taskflow/taskflow.hpp"

// Dynamic Memory Allocation
#include "vk_mem_alloc.h"

#include "stb_image.h"

#include "tiny_obj_loader.h"


// Structures used in all the project
struct Vertex{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex_coord;

    // Info needed to tell VUlkan how to pass Vertex data to the shader
    static vk::VertexInputBindingDescription getBindingDescription() {
        // First is index, second is stride, last means either read data
        // one vertex at the time, or one instance at the time
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        return{
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, tex_coord))
        };
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && tex_coord == other.tex_coord;
    }
};

namespace std {
    template<> struct hash<glm::vec2> {
        std::size_t operator()(const glm::vec2& v) const {
            return std::hash<float>()(v.x) ^ (std::hash<float>()(v.y) << 1);
        }
    };

    template<> struct hash<glm::vec3> {
        std::size_t operator()(const glm::vec3& v) const {
            return std::hash<float>()(v.x) ^ (std::hash<float>()(v.y) << 1) ^ (std::hash<float>()(v.z) << 2);
        }
    };

    // Custom specialization of std::hash for Vertex
    template<> struct hash<Vertex> {
        std::size_t operator()(const Vertex& vertex) const {
            std::size_t h1 = std::hash<glm::vec3>()(vertex.pos);
            std::size_t h2 = std::hash<glm::vec3>()(vertex.color);
            std::size_t h3 = std::hash<glm::vec2>()(vertex.tex_coord);

            // Combine the hash values
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}


struct AllocatedBuffer{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct AllocatedImage{
    VkImage image;
    VmaAllocation allocation;
    VkImageView image_view;
    VkExtent3D image_extent;
    VkFormat image_format;
    uint32_t mip_levels;
};


struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    std::optional<uint32_t> transfer_family;

    bool isComplete() const{
        return graphics_family.has_value() && 
            present_family.has_value() &&
            transfer_family.has_value();
    }
};


// ------ Helper Functions

static const char* VmaResultToString(VkResult r);

vk::raii::CommandBuffer beginSingleTimeCommands(vk::raii::CommandPool& command_pool, vk::raii::Device *logical_device);

void endSingleTimeCommands(vk::raii::CommandBuffer& command_buffer, vk::raii::Queue& queue);

vk::Format findSupportedFormat(vk::raii::PhysicalDevice& physical_device, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

bool hasStencilComponent(vk::Format format);

vk::Format findDepthFormat(vk::raii::PhysicalDevice& physical_device);

// ------ General Functions

void createBuffer(
    VmaAllocator& vma_allocator,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    AllocatedBuffer &allocated_buffer);

void copyBuffer(
    VkBuffer &src_buffer,VkBuffer &dst_buffer, vk::DeviceSize size,
    vk::raii::CommandPool& command_pool,
    vk::raii::Device *logical_device,
    vk::raii::Queue& queue
);

void copyBufferToImage(
    const VkBuffer& buffer,
    VkImage& image,
    uint32_t width,
    uint32_t height,
    vk::raii::Device *logical_device, 
    vk::raii::CommandPool &command_pool, 
    vk::raii::Queue &queue
);

