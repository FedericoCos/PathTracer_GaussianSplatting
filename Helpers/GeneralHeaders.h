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
#include <cmath>
#include <fstream>
#include <tuple>
#include <map>
#include <stack>
#include <utility>
#include <bits/stdc++.h>

#include <vulkan/vulkan_raii.hpp> // this library handles for us the vkCreateXXX
                                  // vkAllocateXXX, vkDestroyXXX, and vkFreeXXX
#include <vulkan/vk_platform.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>


// Taskflow is used for multithreading on the CPU side
#include "taskflow/taskflow.hpp"

// Dynamic Memory Allocation
#include "vk_mem_alloc.h"

#include "stb_image.h"

#include "tiny_obj_loader.h"

#include "tinygltf-release/tiny_gltf.h"

#include "json.hpp"


using json = nlohmann::json;


// Structures used in all the project
enum class TransparencyMode{
    OPAQUE,
    OIT_WRITE,
    OIT_COMPOSITE,
    POINTCLOUD
};


struct Vertex{
    glm::vec3 pos;
    float pad1;
    glm::vec3 normal;
    float pad2;
    glm::vec3 color;
    float pad3;
    glm::vec4 tangent;
    glm::vec2 tex_coord;
    glm::vec2 tex_coord_1;

    // Info needed to tell VUlkan how to pass Vertex data to the shader
    static vk::VertexInputBindingDescription getBindingDescription() {
        // First is index, second is stride, last means either read data
        // one vertex at the time, or one instance at the time
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 6> getAttributeDescriptions() {
        return{
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, tangent)),
            vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, tex_coord)),
            vk::VertexInputAttributeDescription(5, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, tex_coord_1)),
        };
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && tex_coord == other.tex_coord && tangent == other.tangent && normal == other.normal;
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

    template<> struct hash<glm::vec4> {
        std::size_t operator()(const glm::vec4& v) const {
            return ((std::hash<float>()(v.x) ^
                   (std::hash<float>()(v.y) << 1)) >> 1) ^
                   ((std::hash<float>()(v.z) ^
                   (std::hash<float>()(v.w) << 1)) >> 1);
        }
    };

    // Custom specialization of std::hash for Vertex
    template<> struct hash<Vertex> {
        std::size_t operator()(const Vertex& vertex) const {
            return ((std::hash<glm::vec3>()(vertex.pos) ^
                   (std::hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (std::hash<glm::vec3>()(vertex.normal) << 1) ^
                   (std::hash<glm::vec4>()(vertex.tangent) << 1) ^
                   (std::hash<glm::vec2>()(vertex.tex_coord) << 1);
        }
    };
}

/**
 * Structure for enignes buffers
 * Cleanup itself when going out of scope
 */
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

struct Material {
    // Indices into the Gameobject's main texture list
    int albedo_texture_index = 0;
    int normal_texture_index = 0;
    int metallic_roughness_texture_index = 0;
    int occlusion_texture_index = 0;
    int emissive_texture_index = 0;  
    int transmission_texture_index = 0;
    int clearcoat_texture_index = 0;
    int clearcoat_roughness_texture_index = 0;

    // PBR scalar factors (loaded from glTF)
    glm::vec4 base_color_factor = glm::vec4(1.0f);
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    glm::vec3 emissive_factor = glm::vec3(0.0f);
    float occlusion_strength = 1.0f;
    glm::vec3 specular_color_factor = glm::vec3(1.f);
    float specular_factor = 0.5f;
    float transmission_factor = 0.0f;
    float alpha_cutoff = 0.0f;
    float clearcoat_factor = 0.0f;
    float clearcoat_roughness_factor = 0.0f;
    int specular_glossiness_texture_index = -1;
    float use_specular_glossiness_workflow = 0.0f;

    bool is_transparent = false;
    bool is_doublesided = false;

    glm::mat4 uv_normal = glm::mat4(1.0f);
    glm::mat4 uv_emissive = glm::mat4(1.0f);
    glm::mat4 uv_albedo = glm::mat4(1.0f);
};

struct MaterialPushConstant {
    glm::vec4 base_color_factor;          // 16

    glm::mat4 uv_normal;
    glm::mat4 uv_emissive;
    glm::mat4 uv_albedo;

    glm::vec4 emissive_factor_and_pad;    // 16

    float   metallic_factor;              // 4
    float   roughness_factor;             // 4
    float   occlusion_strength;           // 4  
    float specular_factor;                // 4

    glm::vec3 specular_color_factor;      // 12
    float alpha_cutoff;                   // 4

    float transmission_factor;            // 4
    float clearcoat_factor;               // 4
    float clearcoat_roughness_factor;     // 4
    float pad;                            // 4


    int albedo_texture_index;
    int normal_texture_index;
    int metallic_roughness_texture_index;
    int emissive_texture_index;
    int occlusion_texture_index;
    int clearcoat_texture_index;
    int clearcoat_roughness_texture_index;

    int sg_id = -1;
    float use_specular_glossiness_workflow = 0.0f;
};

struct Primitive {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    int material_index = -1;

    glm::vec3 center;
};

struct TorusConfig {
    float major_radius = 16.0f;
    float minor_radius = 1.0f;
    float height = 8.0f;
    int major_segments = 500;
    int minor_segments = 500;
};

struct PunctualLight {
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float range;
    glm::vec3 direction;
    float outer_cone_cos;
    float inner_cone_cos;
    int type;
    glm::vec2 padding;
};


struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec3 camera_pos;
    uint32_t frame_count = 0;

    glm::vec4 ambient_light;

    float emissive_flux = 0.0f;
    float punctual_flux = 0.f; 
    float total_flux;
    float p_emissive = 0.f;

    float fov = 60.f;
    float height = 720.f;
    glm::vec2 pad;
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
    RESET,
    SWITCH,

    MAJ_RAD_UP, MAJ_RAD_DOWN,
    MIN_RAD_UP, MIN_RAD_DOWN,
    HEIGHT_UP, HEIGHT_DOWN,

    POINTCLOUD,
    F_POINTCLOUD,
    TOGGLE_PROJECTION,

    CAPTURE_DATA,

    SAMPLING_METHOD
};

/**
 * Structure that holds user input status
 */
struct InputState{
    glm::vec2 move{0.f, 0.f};
    float look_x = 0.f;
    float look_y = 0.f;

    bool consumed = false;

    // For normal camera
    bool speed_up = false;
    bool speed_down = false;
    bool rot_up = false;
    bool rot_down = false;
    bool fov_up = false;
    bool fov_down = false;

    // To switch between the two cameras
    bool reset = false;
    bool change = false;

    // For torus obj
    bool maj_rad_up = false;
    bool maj_rad_down = false;
    bool min_rad_up = false;
    bool min_rad_down = false;
    bool height_up = false;
    bool height_down = false;

    bool left_mouse = false;
};

struct ImageReadbackData {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
};

struct FreeCamera{
    glm::vec3 position = glm::vec3(0.f, 1.5f, 8.f);
    glm::vec3 direction = glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
    float speed = 4.5f;
    float sensitivity = .3f;
};

struct ToroidalCamera{
    glm::vec3 position;
    float alpha = 0.f;
    float beta = 0.f;
    float alpha_speed = 20.f;
    float beta_speed = 20.f;
};

struct CameraState{
    bool is_toroidal = true;
    FreeCamera f_camera;
    ToroidalCamera t_camera;

    glm::mat4 view_matrix;
    glm::mat4 projection_matrix;

    float fov = 60.f;
    float near_plane = 0.1f;
    float far_plane = 10000.f;
    float aspect_ratio;
};

struct PipelineKey {
    std::string v_shader;
    std::string f_shader;
    TransparencyMode mode;
    vk::CullModeFlagBits cull_mode;

    bool operator<(const PipelineKey& other)const {
        return std::tie(v_shader, f_shader, mode, cull_mode) <
                std::tie(other.v_shader, other.f_shader, other.mode, other.cull_mode);
    }
};

struct PipelineInfo {
    vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
    vk::raii::Pipeline pipeline = nullptr;
    vk::raii::PipelineLayout layout = nullptr;
    std::string v_shader;
    std::string f_shader;
    bool is_transparent;
    vk::CullModeFlagBits cull_mode;

    PipelineInfo() = default;

    // Move Constructor
    PipelineInfo(PipelineInfo&& other) noexcept
        : descriptor_set_layout(std::move(descriptor_set_layout)), pipeline(std::move(other.pipeline)), layout(std::move(other.layout)),
            v_shader(other.v_shader), f_shader(other.f_shader), is_transparent(other.is_transparent),
            cull_mode(other.cull_mode) {}

    // Move Assignment Operator
    PipelineInfo& operator=(PipelineInfo&& other) noexcept {
        if (this != &other) {
            descriptor_set_layout = std::move(other.descriptor_set_layout);
            pipeline = std::move(other.pipeline);
            layout = std::move(other.layout);
            v_shader = other.v_shader;
            f_shader = other.f_shader;
            is_transparent = other.is_transparent;
            cull_mode = other.cull_mode;
        }
        return *this;
    }

    // --- Disable Copying ---
    PipelineInfo(const PipelineInfo&) = delete;
    PipelineInfo& operator=(const PipelineInfo&) = delete;
};

struct AccelerationStructure {
    vk::raii::AccelerationStructureKHR as = nullptr;
    AllocatedBuffer buffer;
    uint64_t device_address = 0;
};

struct RayTracingProperties {
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR pipeline_props;
    vk::PhysicalDeviceAccelerationStructurePropertiesKHR as_props;
};

struct MeshInfo {
    uint32_t material_index; // Index into all_materials_buffer
    uint32_t vertex_offset;  // Offset into all_vertices_buffer
    uint32_t index_offset;   // Offset into all_indices_buffer
    uint32_t _pad1;          // for 16-byte alignment
};

struct RaySample {
    glm::vec2 uv;
};

struct RayPushConstant {
    glm::mat4 model;
    int mode;
    float major_radius; 
    float minor_radius;
    float height;
};

struct PC { 
    glm::mat4 model; 
    int mode; 
    float major_radius; 
    float minor_radius;
    float height;
}; 

enum class SamplingMethod {
    HALTON,
    STRATIFIED,
    IMP_COL,
    RANDOM,
    UNIFORM,
    IMP_HIT,
    LHS
};

const std::array<SamplingMethod, 8> sampling_methods = {
    SamplingMethod::RANDOM,
    SamplingMethod::UNIFORM,
    SamplingMethod::STRATIFIED,
    SamplingMethod::LHS,
    SamplingMethod::HALTON,
    SamplingMethod::IMP_COL,
    SamplingMethod::HALTON,
    SamplingMethod::IMP_HIT,
};

/**
 * Structure fille by the GPU to fill the buffer for the sampling methods and the pointcloud
 */
struct HitDataGPU { 
    glm::vec3 pos;
    // float hit_flag
    float flag;
    
    glm::vec4 color;
    
    // vec3 normal
    glm::vec3 normal;
    // float padding
    float padding;
};

struct FrameData {
    std::string file_path;
    glm::mat4 transform_matrix; // Camera-to-World
};

struct EmissiveTriangle {
    // Indices into the object's index buffer
    uint32_t index0;
    uint32_t index1;
    uint32_t index2;
    // Material index (to get emission color/strength)
    uint32_t material_index;
    // Area (pre-calculated on CPU for importance sampling)
    float area;
};

struct LightTriangle {
    uint32_t v0, v1, v2;
    uint32_t material_index; 
};

struct LightCDF {
    float cumulative_probability; // [0.0 to 1.0]
    uint32_t triangle_index;      // Index into LightTriangle buffer
    float padding[2];
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

void readBuffer(vk::Buffer buffer, vk::DeviceSize size, void* dst_ptr,
                VmaAllocator &vma_allocator,
                vk::raii::Device *logical_device, vk::raii::CommandPool &command_pool,
                vk::raii::Queue &queue);

void savePNG(const std::string& filename, const ImageReadbackData& data);

void saveJPG(const std::string& filename, const ImageReadbackData& data, int quality = 90);

