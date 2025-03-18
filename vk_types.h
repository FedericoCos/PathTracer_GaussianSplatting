// The entire codebase includes this header. It provides widely used default structures and includes

// Prepocessor directive that tells the compiler 
// to never include this twice into the sam file
#pragma once

// Main header for vulkan
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"
struct DescriptorAllocatorGrowable; // forward declaration of structure
#include <deque>
#include <bits/stdc++.h>
#include <vector>
#include <span>
#include <iostream>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct DeletionQueue{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function){ // && indicates an rvalue reference, allowing to move (rather than copy) a function into the deque to avoid unnecessary copying
        deletors.push_back(function);
    }

    /**
     * TODO
     * Doing backcalls is inefficient at scale, because we are storing whole std::functions for every object we are deleting
     * which is not going to be optimal. If you need to delete thousands
     * of objects, a better implementation would be to store arrays of vulkan handles of various types
     * such as VkImage, VkBuffer, and so on, and then delete those from a loop
     */
    void flush(){
        // reverse iterate the deletion queue to execute all the functions, start from last element aand going backwards
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++){
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct FrameData{
    VkCommandPool _commandPool; // the command pool for the commands
    VkCommandBuffer _mainCommandBuffer; // the buffer to record into

    VkSemaphore _swapchainSemaphore, // so that render commands wait on the swapchain image request
                _renderSemaphore; // control presenting the image to the OS once the drawing finishes
    VkFence _renderFence; // waits for the draw commands of a given frame to be finished
    DeletionQueue _deletionQueue;

    DescriptorAllocatorGrowable * _frameDescriptors; // resetting the whole descriptor pool
                                                    // at once is a lot faster than trying
                                                    // to keep track of individual descriptor
                                                    // set resource lifetimes
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer{
    VkBuffer buffer; // Vulkan handle for the buffer
    /**
     * The following are used to free the buffer
     */
    VmaAllocation allocation; // contains metadata abouth the buffer
    VmaAllocationInfo info; // contains allocation of the buffer
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct GPUSceneData{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

enum class MaterialPass :uint8_t{
    MainColor,
    Transparent,
    Other
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline * pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

struct DrawContext;

// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        // draw children
        for (auto& c : children) {
            c->Draw(topMatrix, ctx);
        }
    }
};
