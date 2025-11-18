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

const int MAX_SHADOW_LIGHTS = 10;
const int MAX_BINDLESS_TEXTURES = 1024;



const std::vector validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;


struct TransparentDraw {
    Gameobject* object; // Base pointer to the game object
    const Primitive* primitive;
    const Material* material;
    float distance_sq; // Squared distance from camera

    // Sort back-to-front (farthest first)
    bool operator<(const TransparentDraw& other) const {
        return distance_sq > other.distance_sq;
    }
};


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


    // OIT variables
    /* AllocatedImage oit_accum_image; // MSAA Accumulation buffer
    AllocatedImage oit_reveal_image; // MSAA revealage buffer
    AllocatedImage oit_accum_resolved; // Resolved (non-MSAA) accum
    AllocatedImage oit_reveal_resolved; // Resolved (non-MSAA) reveal
    vk::raii::Sampler oit_sampler = nullptr;

    PipelineInfo oit_composite_pipeline;
    std::vector<vk::raii::DescriptorSet> oit_composite_descriptor_sets; */

    AllocatedBuffer oit_atomic_counter_buffer;
    AllocatedBuffer oit_fragment_list_buffer;
    AllocatedImage oit_start_offset_image;
    uint32_t oit_max_fragments;

    std::vector<vk::raii::DescriptorSet> oit_ppll_descriptor_sets;
    PipelineInfo oit_composite_pipeline;

    // Uniform Buffer
    UniformBufferObject ubo{};

    std::array<AllocatedImage, MAX_SHADOW_LIGHTS> shadow_maps;
    vk::raii::Sampler shadow_sampler = nullptr;
    PipelineInfo shadow_pipeline;
    const uint32_t SHADOW_MAP_DIM = 1024;
    vk::Format shadow_map_format;
    bool panel_shadows_enabled = true; 
    float shadow_light_far_plane = 100.f;

    struct ShadowUBO {
        glm::mat4 proj;
        std::array<glm::mat4, 6> views;
        glm::vec4 lightPos;
        float farPlane;
    } shadow_ubo_data; // One struct for temp data
    
    // We need 5 UBOs per frame
    std::array<std::vector<AllocatedBuffer>, MAX_FRAMES_IN_FLIGHT> shadow_ubos;
    std::array<std::vector<void*>, MAX_FRAMES_IN_FLIGHT> shadow_ubos_mapped;
    
    // We need 5 descriptor sets per frame
    std::array<std::vector<vk::raii::DescriptorSet>, MAX_FRAMES_IN_FLIGHT> shadow_descriptor_sets;


    // Ray Tracing TLAS members
    AccelerationStructure tlas;
    AllocatedBuffer tlas_instance_buffer;
    void * tlas_instance_buffer_mapped = nullptr;
    AllocatedBuffer tlas_scratch_buffer;
    uint64_t tlas_scratch_addr = 0;
    const uint32_t MAX_TLAS_INSTANCES = 1024;

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

    // Used to track GPU resources usage
    void printGpuMemoryUsage();

    // --- Public Helper functions
    uint64_t getBufferDeviceAddress(vk::Buffer buffer);


private:
    // Window variables
    uint32_t win_width = 1280;
    uint32_t win_height = 800;

    float total_elapsed = 0.f;
    int fps_count = 0;

    // RAII context
    vk::raii::Context context;

    // Pipeline variables
    std::map<PipelineKey, std::vector<Gameobject>> p_o_map;
    std::map<PipelineKey, PipelineInfo> p_p_map;
    const std::string v_shader_pbr = "shaders/basic/vertex.spv";
    const std::string f_shader_pbr = "shaders/basic/fragment.spv";
    const std::string f_shader_oit_write = "shaders/basic/oit_ppll_write.spv"; 
    const std::string v_shader_oit_composite = "shaders/basic/oit_composite_vert.spv"; 
    const std::string f_shader_oit_composite = "shaders/basic/oit_ppll_composite_frag.spv"; 
    
    const std::string v_shader_torus = "shaders/basic/vertex_torus.spv";
    const std::string f_shader_torus = "shaders/basic/fragment_torus.spv";

    const std::string v_shader_shadow = "shaders/basic/shadow_vert.spv";
    const std::string f_shader_shadow = "shaders/basic/shadow_frag.spv";

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
    std::vector<P_object> scene_objs;
    std::vector<TransparentDraw> transparent_draws;
    Gameobject debug_cube;

    Gameobject createDebugCube();
    void createRTBox(const std::string& rtbox_path);

    // For multisampling
    AllocatedImage color_image;

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

        // LIGHTS
        {GLFW_KEY_Z, Action::DEBUG_LIGHTS},
        {GLFW_KEY_5, Action::TOGGLE_FLOOR_LIGHT},
        {GLFW_KEY_6, Action::TOGGLE_CEILING_LIGHT},
        {GLFW_KEY_7, Action::TOGGLE_BACK_LIGHT},
        {GLFW_KEY_8, Action::TOGGLE_LEFT_LIGHT},
        {GLFW_KEY_9, Action::TOGGLE_RIGHT_LIGHT},
        {GLFW_KEY_E, Action::TOGGLE_EMISSIVE},
        {GLFW_KEY_Q, Action::TOGGLE_MANUAL},

        {GLFW_KEY_P, Action::POINTCLOUD},

    };
    std::unordered_set<int> pressed_keys;

    // Lights
    bool debug_lights = false;
    Gameobject rt_box;
    bool use_rt_box = false;

    // Light Storage
    std::vector<Pointlight> manual_lights;
    std::vector<bool> manual_lights_shadow;
    std::vector<Pointlight> emissive_lights;
    std::vector<Pointlight> panel_lights;
    std::vector<bool> panel_lights_on;

    // Light Toggles
    bool use_manual_lights = false;
    bool use_emissive_lights = false;
    bool use_manual_lights_shadows = false;
    bool use_emissive_lights_shadows = false;

    bool render_point_cloud = false;

    // --- Ray Tracing Function Pointers ---
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;


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

    // ----- Init functions
    bool initWindow();
    bool initVulkan();

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

    void createOITResources();
    // void destroyOITResources(); // to later implement
    void createOITCompositePipeline();
    void createOITDescriptorSets();

    void createShadowResources();
    void createShadowPipeline();

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

    // ---- Closing functions
    void cleanup();

};


