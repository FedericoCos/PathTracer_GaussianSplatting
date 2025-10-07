#pragma once

#include "GLFWhelper.h"

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


struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocator* p_allocator = nullptr;
    VmaAllocationInfo info = {};

    AllocatedBuffer() = default;

    // Destructor handles cleanup automatically!
    ~AllocatedBuffer() {
        if (buffer != VK_NULL_HANDLE && p_allocator != nullptr) {
            vmaDestroyBuffer(*p_allocator, buffer, allocation);
        }
    }

    // Disable copy
    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

    // Move constructor
    AllocatedBuffer(AllocatedBuffer&& other) noexcept
        : buffer(other.buffer),
          allocation(other.allocation),
          p_allocator(other.p_allocator),
          info(other.info) {
        // Reset other to safe state
        other.buffer = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.p_allocator = nullptr;
        other.info = {};
    }

    // Move assignment
    AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept {
        if (this != &other) {
            // Clean up existing
            if (buffer != VK_NULL_HANDLE && p_allocator != nullptr) {
                vmaDestroyBuffer(*p_allocator, buffer, allocation);
            }

            // Transfer ownership
            buffer = other.buffer;
            allocation = other.allocation;
            p_allocator = other.p_allocator;
            info = other.info;

            // Reset other
            other.buffer = VK_NULL_HANDLE;
            other.allocation = VK_NULL_HANDLE;
            other.p_allocator = nullptr;
            other.info = {};
        }
        return *this;
    }
};

struct AllocatedImage{
    VkImage image;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
    uint32_t mip_levels;
    vk::raii::ImageView image_view{nullptr};
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

struct SwapChainBundle{
    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> image_views;
    vk::Format format;
    vk::Extent2D extent;
};

enum class Action{
    MOVE_LEFT, MOVE_RIGHT, MOVE_FORWARD, MOVE_BACKWARD,
    SPEED_UP, SPEED_DOWN, ROT_UP, ROT_DOWN,
    FOV_UP, FOV_DOWN,
    RADIUS_UP, RADIUS_DOWN,
    HEIGHT_UP, HEIGHT_DOWN,
    RESET,
    SWITCH
};

struct InputState{
    glm::vec2 move{0.f, 0.f};
    float look_x = 0.f;
    float look_y = 0.f;

    bool consumed = false;
    bool speed_up = false;
    bool speed_down = false;
    bool rot_up = false;
    bool rot_down = false;
    bool fov_up = false;
    bool fov_down = false;
    bool radius_up = false;
    bool radius_down = false;
    bool height_up = false;
    bool height_down = false;
    bool reset = false;
    bool change = false;

    bool left_mouse = false;
};

struct FreeCamera{
    glm::vec3 position = glm::vec3(0.f, 1.5f, 8.f);
    glm::vec3 direction = glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
    float speed = 2.5f;
    float sensitivity = .002f;
};

struct ToroidalCamera{
    glm::vec3 postion;
    float radius = 12.f;
    float alpha = 0.f;
    float beta = 0.f;
    float alpha_speed = .2f;
    float beta_speed = .2f;
    float height = .5f;
};

struct CameraState{
    bool is_toroidal = true;
    FreeCamera f_camera;
    ToroidalCamera t_camera;

    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;

    float fov = 45.f;
    float near_plane = 0.1f;
    float far_plane = 100.f;
    float aspect_ratio;
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

