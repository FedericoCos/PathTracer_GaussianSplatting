// The entire codebase includes this header. It provides widely used default structures and includes

// Prepocessor directive that tells the compiler 
// to never include this twice into the sam file
#pragma once

// Main header for vulkan
#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"
#include <deque>
#include <bits/stdc++.h>
#include <vector>
#include <span>
#include <iostream>

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
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

