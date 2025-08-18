#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

// GLFW is for window and input management
#include "../Helpers/GLFWhelper.h"

// All the Engine pieces
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"



const std::vector validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


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
    vk::raii::CommandBuffer command_buffer = nullptr;

    // Synchronization variables
    vk::raii::Semaphore present_complete_semaphore = nullptr;
    vk::raii::Semaphore render_finished_semaphore = nullptr;
    vk::raii::Fence draw_fence = nullptr;


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

    // ----- Init functions
    bool initWindow();
    bool initVulkan();

    void createInstance();
    void createSurface();
    void createCommandPool();
    void createCommandBuffer();
    void createSyncObjects();

    // Drawing functions
    void recordCommandBuffer(uint32_t image_index);
    void drawFrame();


    // ---- Closing functions
    void cleanup();

};


