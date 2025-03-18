// This will be the main class for the engine, and where most of the code will go

#pragma once

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_loader.h"

#include "VkBootstrap.h"

#include <deque>
#include <bits/stdc++.h>

constexpr unsigned int FRAME_OVERLAP = 2; // double buffering: GPU running some commands while we write into others

// Push constant for gradient_color shader
struct ComputePushConstants{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;

};

struct ComputeEffect{
    const char * name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

struct GLTFMetallic_Roughness{
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        // padding, we need it anyway for uniform buffers
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void build_pipelines(VulkanEngine * engine);
    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};

class VulkanEngine{
public:

    bool _isInitialized{ false };
    int _frameNumber{0};
    bool stop_rendering{ false };
    int _version{3};

    VkExtent2D _windowExtent{ 1700, 900 };
    struct SDL_Window * _window{ nullptr };

    // Initialization variables
    VkInstance _instance; // Vulkan library handle
    VkDebugUtilsMessengerEXT _debug_messanger; // Vulkan debug output handle
    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device; // Vulkan device for commands
    VkSurfaceKHR _surface; // Vulkan window surface

    // Swapchain variables
    VkSwapchainKHR _swapchain; 
    VkFormat _swapchainImageFormat; // image format expected by the windowing system
    std::vector<VkImage> _swapchainImages; // array of images from the swapchain to use as texture or to render into
    std::vector<VkImageView> _swapchainImageViews; // array of image-views from the swapchain. Wrappers for images

    // Command variables
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily; // family of the above queue
    FrameData _frames[FRAME_OVERLAP];

    DeletionQueue _mainDeletionQueue;

    // Virtual memory allocator
    VmaAllocator _allocator;

    // draw resources
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;

    // DescriptorsSets variables
    DescriptorAllocatorGrowable globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    // gradient (compute) pipeline
    VkPipeline _gradientPipeline;
    VkPipelineLayout _gradientPipelineLayout;

    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect{0};

    // immediate submit structures (for ImGui)
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    // Pipeline for triangle shaders
    VkPipelineLayout _trianglePipelineLayout;
    VkPipeline _trianglePipeline;

    // pipelined for meshed triangle shaders
    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _meshPipeline;

    GPUMeshBuffers rectangle;

    // for window resizeing
    bool resize_requested = false;
    float renderScale = 1.f;

    // Descriptor layout for scende data
    GPUSceneData sceneData;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    //Basic test images and samplers
    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    // Descriptor Set Layout for single image
    VkDescriptorSetLayout _singleImageDescriptorLayout;

    // meshes
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    // Material Variables
    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;

    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

    void update_scene();


    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();

    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

    //run main loop
    void run();

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    static VulkanEngine& Get();

    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:

    // Initialization functions
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipelines();
    void init_background_pipelines();
    void init_imgui();
    void draw_background(VkCommandBuffer cmd);
    void init_triangle_pipeline();
    void draw_geometry(VkCommandBuffer cmd);
    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer& buffer);
    void init_mesh_pipeline();
    void init_default_data();
    void resize_swapchain();
    void destroy_swapchain();
    void create_swapchain(uint32_t width, uint32_t height);
    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    void destroy_image(const AllocatedImage& img);
};