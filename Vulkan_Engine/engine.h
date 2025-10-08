#pragma once

// Base class for the engine
#include "../Helpers/GeneralHeaders.h"

// All the Engine pieces
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"
#include "image.h"
#include "camera.h"
#include "torus.h"
#include "gameobject.h"
#include "p_object.h"

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

    // Window variables
    GLFWwindow *window;

    // Instance variable
    vk::raii::Instance instance = nullptr;

    // Surface variables
    vk::raii::SurfaceKHR surface = nullptr;

    // Memory variable
    VmaAllocator vma_allocator;

    // Device Variables
    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device logical_device = nullptr;
    QueueFamilyIndices queue_indices;

    // Device variables
    vk::raii::Queue graphics_queue = nullptr;
    vk::raii::Queue present_queue = nullptr;
    vk::raii::Queue transfer_queue = nullptr;

    // Swapchain variables
    SwapChainBundle swapchain;

    // Pipeline variables
    vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;

    // Command pools variables
    vk::raii::CommandPool command_pool_graphics = nullptr;
    std::vector<vk::raii::CommandBuffer> graphics_command_buffer;
    vk::raii::CommandPool command_pool_transfer = nullptr;



    // For multisampling
    vk::SampleCountFlagBits mssa_samples = vk::SampleCountFlagBits::e1;


private:

    // Window variables
    uint32_t win_width = 1280;
    uint32_t win_height = 800;

    const std::string MODEL_PATH = "resources/Models/Test_models/HouseSuburban.obj";
    const std::string TEXTURE_PATH = "resources/Models/Test_models/HouseSuburban_Base.png";

    // RAII context
    vk::raii::Context context;

    // Pipeline variables
    vk::raii::PipelineLayout graphics_pipeline_layout = nullptr;
    vk::raii::Pipeline graphics_pipeline = nullptr;

    std::map<std::string, PipelineInfo> shader_pipelines;
    std::map<PipelineInfo, std::vector<Gameobject>> pipelines_obj;

    // Texture variables
    AllocatedImage texture;
    vk::raii::Sampler texture_sampler = nullptr;

    AllocatedImage depth_image;

    // Synchronization variables
    std::vector<vk::raii::Semaphore> present_complete_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    // Frame variables
    uint32_t semaphore_index = 0;
    uint32_t current_frame = 0;
    bool framebuffer_resized = false;


    // Scene objects
    Torus torus;
    P_object house;

    // Uniform buffer variables
    std::vector<AllocatedBuffer> uniform_buffers;
    std::vector<void *> uniform_buffers_mapped;

    // Descriptor Pool variables
    vk::raii::DescriptorPool descriptor_pool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptor_sets;

    // For multisampling
    AllocatedImage color_image;

    // Camera
    Camera camera;

    // For tracking time updates and events
    std::chrono::_V2::system_clock::time_point prev_time;
    InputState input;
    std::unordered_map<int, Action> key_mapping = {
        {GLFW_KEY_A, Action::MOVE_LEFT},
        {GLFW_KEY_D, Action::MOVE_RIGHT},
        {GLFW_KEY_W, Action::MOVE_FORWARD},
        {GLFW_KEY_S, Action::MOVE_BACKWARD},
        {GLFW_KEY_UP, Action::SPEED_UP},
        {GLFW_KEY_DOWN, Action::SPEED_DOWN},
        {GLFW_KEY_RIGHT, Action::ROT_UP},
        {GLFW_KEY_LEFT, Action::ROT_DOWN},
        {GLFW_KEY_L, Action::FOV_UP},
        {GLFW_KEY_K, Action::FOV_DOWN},
        {GLFW_KEY_P, Action::RADIUS_UP},
        {GLFW_KEY_O, Action::RADIUS_DOWN},
        {GLFW_KEY_M, Action::HEIGHT_UP},
        {GLFW_KEY_N, Action::HEIGHT_DOWN},
        {GLFW_KEY_R, Action::RESET},
        {GLFW_KEY_C, Action::SWITCH},
    };
    std::unordered_set<int> pressed_keys;


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
    vk::SampleCountFlagBits getMaxUsableSampleCount();
    void createModel(Gameobject &obj);


    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
    static void cursor_position_callback(GLFWwindow *window, double x_pos, double y_pos);

    // ----- Init functions
    bool initWindow();
    bool initVulkan();

    void createInstance();
    void createSurface();

    void createCommandPool();
    void createGraphicsCommandBuffers();

    void createSyncObjects();
    void loadModel();
    void createDataBuffer();
    void createToroidModel();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();

    // Drawing functions
    void recordCommandBuffer(uint32_t image_index);
    void drawFrame();
    void updateUniformBuffer(uint32_t current_image);

    void recreateSwapChain();


    // ---- Closing functions
    void cleanup();

};


