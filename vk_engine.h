// main class for the engine

#pragma once

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_images.h"

#include "VkBootstrap.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"
#include "imgui/imgui_impl_vulkan.h"

#include "camera.h"
#include "scene.h"

class VulkanScene; // forwared declaration

const int FRAME_OVERLAP = 2;

bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);

class VulkanEngine{
private:
    // -----------------------------------  INITIALIZATION VARIABLES
    VkExtent2D _windowExtent{ 1700, 900 };
    struct SDL_Window * _window{ nullptr };

    DeletionQueue _mainDeletionQueue;

    // core vulkan structures
    VkInstance _instance;
    const bool _useValidationLayer { true };
    const int _version { 3 };
    VkDebugUtilsMessengerEXT _debug_messanger;

    VkDevice _device;
    VkPhysicalDevice _physicalDevice; 
    VkSurfaceKHR _surface;
    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;
    VkQueue _presentQueue;
    uint32_t _presentQueueFamily;

    // Virtual memory allocator
    VmaAllocator _allocator;

    // Swapchain vairables
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    // images variables
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;

    // command buffers for imgui and immediate submits
    VkCommandBuffer _imgCommandBuffer;
    VkCommandPool _imgCommandPool;

    // Fence for imgui and immediate operations
    VkFence _imgFence;

    // Descriptor variables
    DescriptorAllocatorGrowable _globalDescriptorAllocator;
    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
    VkDescriptorSetLayout _singleImageDescriptorLayout;

    // meshes
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    // Textures
    AllocatedImage _whiteImage;
    AllocatedImage _greyImage;
    AllocatedImage _blackImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerNearest;
    VkSampler _defaultSamplerLinear;

    // Materials
    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;

    // To be Rendered
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

    // PIPELINES
    
    // compute pipelines
    std::vector<ComputeEffect> _computePipelines;
    VkPipelineLayout _computePipelineLayout;
    int _currentComputePipeline{0};

    // holds structures per frame
    FrameData _frames[FRAME_OVERLAP];
    int _frameNumber = 0;

    // Scene variables
    std::vector<VulkanScene> scenes;
    int _sceneIndex{0};

    // ------------------------------------------ DRAW VARIABLES
    Camera _mainCamera;

    bool stop_rendering{false};
    bool resize_requested{false};

    DrawContext _mainDrawContext;
    GPUSceneData _sceneData;

    // for stats
    EngineStats _engineStats;

    float _angle = 0.5f;
    bool _updateStructure{true};
    std::chrono::_V2::system_clock::time_point _currentTime;
    
    
    // ----------------------------------------- INITIALIZATION FUNCTION

    /**
     * Starts core vulkan structures: instance and device
     * Also selects grphics queue and queue family
     */
    void init_vulkan();

    /**
     * Initialize Virtual Memory Allocator
     */
    void init_VMA();

    /**
     * Initialize Swapchain
     * Set image format
     * sync mode
     * image usages
     */
    void init_swapchain();
    SwapchainSupportDetails query_swapchain_support();

    /**
     * Initialize Draw images and depth images
     */
    void init_images();

    /**
     * Initialize commands
     */
    void init_commands();

    /**
     * Initialize synchronization structures
     * Fences for CPU to GPU op
     * Semaphores for GPU to GPU op
     */
    void init_sync_structures();

    /**
     * Initialize descriptors
     * Used by shaders to access resources
     * Creates both for compute shaders and normal rasterization
     */
    void init_descriptors();

    void init_mesh();

    void init_texture();

    void init_materials();

    void init_scene();
    
    void init_pipelines();
    void init_compute_pipeline();

    void init_imgui();

    // --------------------------------------- DRAW FUNCTIONS
    void resize_swapchain();
    void destroy_swapchain();
    void recreate_swapchain();

    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

    void set_data_before_draw();

    void draw();

    void update_scene();
    void draw_background(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

    void set_imgui();


public:
    void init();

    VmaAllocator& getAllocator();
    VkDevice& getDevice();

    /**
     * The VulkanEngine::immediate_submit function is a utility designed to execute a 
     * small batch of Vulkan commands immediately and synchronously. 
     * This is useful for tasks like uploading resources to the GPU, 
     * transitioning image layouts, or other operations that need to be performed 
     * outside the main render loop.
     */
    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    /**
     * Helper function to load a shader
     */
    bool load_shader_module(const char * filePath, VkShaderModule * outShaderModule);

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer& buffer);

    VkDescriptorSetLayout& getGpuSceneDataDescriptorLayout();

    AllocatedImage& getDrawImage();
    AllocatedImage& getDepthImage();

    // Get specific test textures
    AllocatedImage& getErrorCheckerboardImage();
    AllocatedImage& getWhiteImage(){
        return _whiteImage;
    }
    VkSampler& getDefaultSamplerLinear(){
        return _defaultSamplerLinear;
    }

    // Get materials
    GLTFMetallic_Roughness& getMetalRoughMaterial(){
        return metalRoughMaterial;
    }

    MaterialInstance& getDefaultData(){
        return defaultData;
    }


    void run();

};