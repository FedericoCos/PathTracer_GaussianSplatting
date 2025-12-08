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
#include "sampling.h"

#include "vk_mem_alloc.h"

const int MAX_BINDLESS_TEXTURES = 1024;
const int NUM_CAPTURE_POSITIONS = 168;



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


    void init(int mssa_val) {
       initWindow();
       initVulkan(mssa_val);
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

    // Command pools variables
    vk::raii::CommandPool command_pool_graphics = nullptr;
    std::vector<vk::raii::CommandBuffer> graphics_command_buffer;
    vk::raii::CommandPool command_pool_transfer = nullptr;

    // Uniform buffer variables
    std::vector<AllocatedBuffer> uniform_buffers;
    std::vector<void *> uniform_buffers_mapped;

    // Descriptor Pool variables
    vk::raii::DescriptorPool descriptor_pool = nullptr;

    // For multisampling
    vk::SampleCountFlagBits mssa_samples = vk::SampleCountFlagBits::e1;

    // Uniform Buffer
    UniformBufferObject ubo{};


    // Ray Tracing TLAS members
    AccelerationStructure tlas;
    AllocatedBuffer tlas_instance_buffer;
    void * tlas_instance_buffer_mapped = nullptr;
    AllocatedBuffer tlas_scratch_buffer;
    uint64_t tlas_scratch_addr = 0;
    const uint32_t MAX_TLAS_INSTANCES = 1024;

    AllocatedImage rt_output_image;

    // Ray tracing data buffers
    AllocatedBuffer torus_vertex_data_buffer;
    AllocatedBuffer hit_data_buffer;

    // Ray Tracing Pipeline members
    PipelineInfo rt_pipeline;
    std::vector<vk::raii::DescriptorSet> rt_descriptor_sets; // One per frame
    AllocatedBuffer sbt_buffer; // Shader Binding Table
    RayTracingProperties rt_props;

    // RayTarcing function pointers
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

    // Pointcloud Pipeline
    PipelineInfo point_cloud_pipeline;
    std::vector<vk::raii::DescriptorSet> point_cloud_descriptor_sets;

    // Bindless buffer
    std::vector<vk::DescriptorImageInfo> global_texture_descriptors;
    AllocatedBuffer all_materials_buffer;
    AllocatedBuffer all_vertices_buffer;
    AllocatedBuffer all_indices_buffer;
    AllocatedBuffer all_mesh_info_buffer;
    std::vector<PunctualLight> global_punctual_lights;
    AllocatedBuffer punctual_light_buffer;

    // Used to track GPU resources usage
    void printGpuMemoryUsage();

    // --- Public Helper functions
    uint64_t getBufferDeviceAddress(vk::Buffer buffer);


private:
    // Window variables
    uint32_t win_width = 1280;
    uint32_t win_height = 720;

    float total_elapsed = 0.f;
    int fps_count = 0;

    // RAII context
    vk::raii::Context context;


    const std::string rt_rgen_shader = "shaders/rt_datacollect/raygen.rgen.spv";
    const std::string rt_rmiss_shader = "shaders/rt_datacollect/miss.rmiss.spv";
    const std::string rt_rchit_shader = "shaders/rt_datacollect/closesthit.rchit.spv";

    const std::string v_shader_pointcloud = "shaders/pointcloud/pointcloud.vert.spv";
    const std::string f_shader_pointcloud = "shaders/pointcloud/pointcloud.frag.spv";

    // Depth variables
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
    TorusConfig torus_config;
    std::vector<P_object> scene_objs;
    Gameobject debug_cube;

    Gameobject createDebugCube();
    void createRTBox(const std::string& rtbox_path);

    // Camera
    Camera camera;

    // For tracking time updates and events
    std::chrono::_V2::system_clock::time_point prev_time;
    InputState input;
    std::unordered_map<int, Action> key_mapping = {
        // CAMERA
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
        {GLFW_KEY_R, Action::RESET},
        {GLFW_KEY_C, Action::SWITCH},

        // TORUS
        {GLFW_KEY_1, Action::MAJ_RAD_DOWN},
        {GLFW_KEY_2, Action::MAJ_RAD_UP},
        {GLFW_KEY_3, Action::MIN_RAD_DOWN},
        {GLFW_KEY_4, Action::MIN_RAD_UP},
        {GLFW_KEY_M, Action::HEIGHT_UP},
        {GLFW_KEY_N, Action::HEIGHT_DOWN},

        {GLFW_KEY_P, Action::POINTCLOUD},
        {GLFW_KEY_O, Action::F_POINTCLOUD},
        {GLFW_KEY_T, Action::TOGGLE_PROJECTION},

        {GLFW_KEY_V, Action::CAPTURE_DATA},

        {GLFW_KEY_B, Action::SAMPLING_METHOD},

    };
    std::unordered_set<int> pressed_keys;

    // rt Box
    Gameobject rt_box;
    bool use_rt_box = false;

    // Pointcloud settings
    bool render_point_cloud = false;
    bool render_final_pointcloud = true;
    bool show_projected_torus = false;
    bool render_torus = true;
    bool activate_point_cloud = true;

    // For data capturing
    bool is_capturing = false;
    AllocatedImage capture_resolve_image;
    int image_captured_count = 0;
    std::vector<RaySample> sampling_points;
    AllocatedBuffer sample_data_buffer;
    int num_rays = 1000000;
    int current_sampling = 0;
    bool invalid_sampling = true;

    // For emission light
    AllocatedBuffer light_triangle_buffer;
    AllocatedBuffer light_cdf_buffer;
    uint32_t num_light_triangles = 0;
    uint32_t accumulation_frame = -1;

    // Blue noise
    const char * blue_noise_txt_path = "blue_noise/128_128/HDR_LA_0.png";
    AllocatedImage blue_noise_txt;
    vk::DescriptorImageInfo blue_noise_txt_info;
    vk::raii::Sampler blue_noise_txt_sampler = nullptr;

    // --- Ray Tracing Function Pointers ---
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;

    // Ray tracing variables
    uint32_t handle_size;
    uint32_t sbt_entry_size;
    uint64_t sbt_address;
    vk::StridedDeviceAddressRegionKHR rmiss_region;
    vk::StridedDeviceAddressRegionKHR rhit_region;
    vk::StridedDeviceAddressRegionKHR callable_region;


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

    void transitionImage(vk::raii::CommandBuffer& cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspectMask);


    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
    static void cursor_position_callback(GLFWwindow *window, double x_pos, double y_pos);

    void createRTOutputImage();

    // ----- Init functions
    bool initWindow();
    bool initVulkan(int mssa_val);

    void createInstance();
    void createSurface();

    void createCommandPool();
    void createGraphicsCommandBuffers();

    void createSyncObjects();

    void createPipelines();
    void loadScene(const std::string& scene_path);
    void loadManualLights(const std::string& lights_path);
    void createTorusModel();
    void buildBlas(Gameobject& obj);
    void createTlasResources();
    void buildTlas(vk::raii::CommandBuffer& cmd);
    void createRayTracingDataBuffers();

    void createPointCloudPipeline();
    void createPointCloudDescriptorSets();


    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();

    // RT Pipeline Functions
    void createRayTracingPipeline();
    void createRayTracingDescriptorSets();
    void createShaderBindingTable();
    void createGlobalBindlessBuffers();



    // Drawing functions
    void recordCommandBuffer(uint32_t image_index);
    void drawFrame();
    void updateUniformBuffer(uint32_t current_image);

    void recreateSwapChain();

    // Capture data function
    ImageReadbackData readImageToCPU(vk::Image image, VkFormat format, uint32_t width, uint32_t height);
    void captureSceneData();

    // ---- Closing functions
    void cleanup();

    void updateTorusRTBuffer();

    void updateImportanceSampling();
    void saveTransformsJson(const std::string &filename, const std::vector<FrameData> &frames);
    void savePly(const std::string &filename);
    void readBuffer(vk::Buffer buffer, vk::DeviceSize size, void *dst_ptr);
};
