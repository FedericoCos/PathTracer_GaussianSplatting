// contains default structures used in the project

#pragma once

#include <deque>
#include <bits/stdc++.h>
#include <vector>
#include <span>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"

// SwapChain structure
struct SwapchainSupportDetails{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};



struct DescriptorAllocatorGrowable; // forward declaration of structure

/**
 * Queue that holds std::function<void> to destroyed required objects
 */
struct  DeletionQueue{
  std::deque<std::function<void()>> deletors;
  
  void push_function(std::function<void()>&& function){
    deletors.push_back(function);
  }

  void flush(){
    for(auto it = deletors.rbegin(); it != deletors.rend(); it++){
        (*it)();
    }

    deletors.clear();
  }
};

/**
 * Image structure for Vulkan. Holds
 * VkImage
 * VkImageView
 * VmaAllocation
 * VkExtent3D
 * VkFormat
 */
struct AllocatedImage{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

/**
 * Buffer structure for Vulkan. Holds
 * VkBuffer
 * VmaAllocation
 * VmaAllocationInfo
 */
struct AllocatedBuffer
{
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};


/**
 * FrameData structures holds
 * Command pool for the current frame and command buffer
 * 2 sempahores and one fence
 * A Descriptor Allocator
 */
struct FrameData{
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;

    DescriptorAllocatorGrowable * frameDescriptors;
};


/**
 * Vertex structure
 * position
 * uv_x
 * normal
 * uv_y
 * color
 */
struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

/**
 * Push Constant for the compute shader
 */
struct ComputePushConstants{
  glm::vec4 data1;;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

/**
 * Structure to hold a specific compute pipeline, its layouut, and data
 */
struct ComputeEffect{
  const char * name;
  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

/**
 * Push Constant for normal pipeline (mesh objects)
 * Contains the world Matrix and the vertexBuffer address
 */
struct GPUDrawPushConstants {
  glm::mat4 worldMatrix;
  VkDeviceAddress vertexBuffer;
};

/**
 * All the data required to be passed to the shaders
 * Contains
 * view matrix
 * projection matrix
 * viewproj matrix
 * ambient color
 * sun light direction
 * sun light color
 */
struct GPUSceneData{
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection;
  glm::vec4 sunlightColor;
};

/**
 * Structures to keep track of the system
 * contains frametime
 * triangle count
 * drawcall_count
 * scene_update_time
 * mesh_draw_time
 */
struct EngineStats{
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};


struct Bounds
{
  glm::vec3 origin;
  float sphereRadius;
  glm::vec3 extents;
};



