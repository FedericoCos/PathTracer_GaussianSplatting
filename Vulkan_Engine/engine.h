#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

// GLFW is for window and input management
#include "../Helpers/GLFWhelper.h"

// All the Engine pieces
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"

#include "vk_mem_alloc.h"



const std::vector validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;


class Engine{
public:
    Engine(){

    }

    ~Engine(){

    }


    void init() {
       initWindow();
       initVulkan();
    };

    void run();




private:

    // Window variables
    GLFWwindow *window;
    uint32_t win_width = 1280;
    uint32_t win_height = 800;

    // RAII context
    vk::raii::Context context;

    // Instance variable
    vk::raii::Instance instance = nullptr;

    // Memory variable
    VmaAllocator vma_allocator;

    // Device variables
    Device device_obj;
    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device * logical_device;
    vk::raii::Queue graphics_presentation_queue = nullptr;

    // Surface variables
    vk::raii::SurfaceKHR surface = nullptr;

    // Swapchain variables
    Swapchain swapchain_obj;
    vk::raii::SwapchainKHR * swapchain;
    std::vector<vk::Image> swapchain_images;
    vk::Format swapchain_image_format;
    vk::Extent2D swapchain_extent;
    std::vector<vk::raii::ImageView> swapchain_image_views;

    // Pipeline variables
    Pipeline pipeline_obj;
    vk::raii::PipelineLayout* graphics_pipeline_layout = nullptr;
    vk::raii::Pipeline* graphics_pipeline = nullptr;

    // Command pools variables
    vk::raii::CommandPool command_pool = nullptr;
    std::vector<vk::raii::CommandBuffer> command_buffers;
    vk::raii::CommandPool command_pool_copy = nullptr;

    // Synchronization variables
    std::vector<vk::raii::Semaphore> present_complete_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    // Frame variables
    uint32_t semaphore_index = 0;
    uint32_t current_frame = 0;
    bool framebuffer_resized = false;


    // Vertex data
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
    };

    const std::vector<uint16_t> indices = {
        0, 1, 2,
        2, 3, 0
    };
    AllocatedBuffer data_buffer;
    vk::DeviceSize index_offset;


    // ----- Helper functions
    std::vector<const char*> getRequiredExtensions();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

        return vk::False;
    }

    vk::raii::DebugUtilsMessengerEXT debug_messanger = nullptr;
    void setupDebugMessanger();

    void transition_image_layout(
        uint32_t image_index,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask,
        vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask
    );


    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, AllocatedBuffer& buffer_memory);
    void copyBuffer(VkBuffer& src_buffer, VkBuffer& dst_buffer, vk::DeviceSize size);

    // ----- Init functions
    bool initWindow();
    bool initVulkan();

    void createInstance();
    void createSurface();
    void createCommandPool();
    void createCommandBuffer();
    void createSyncObjects();
    void createDataBuffer();

    // Drawing functions
    void recordCommandBuffer(uint32_t image_index);
    void drawFrame();

    void recreateSwapChain();


    // ---- Closing functions
    void cleanup();

};


