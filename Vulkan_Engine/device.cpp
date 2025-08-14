#include "device.h"

bool Device::isDeviceSuitable(vk::raii::PhysicalDevice& device){
    auto device_properties = device.getProperties();
    auto device_features = device.getFeatures();

    // Checking if supported Vulkan is at least 1.3
    if(device_properties.apiVersion < VK_API_VERSION_1_3){
        return false;
    }

    // Picking only Discrete GPU
    if(device_properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu ||
        !device_features.geometryShader){
        return false;
    }


    // Check if any of the queue families support graphics operations
    auto queue_families = device.getQueueFamilyProperties();
    bool supports_graphics = false;
    for(auto const& qfp : queue_families){
        if(!!(qfp.queueFlags & vk::QueueFlagBits::eGraphics)){
            supports_graphics = true;
            break;
        }
    }
    if(!supports_graphics){
        return false;
    }

    // Check if all required device extensions are available
    auto available_device_extensions = device.enumerateDeviceExtensionProperties();
    bool supports_all_required_extensions = true;
    for(auto const& required_ext : device_extensions){
        bool found = false;
        for (auto const& available_ext : available_device_extensions){
            if(strcmp(available_ext.extensionName, required_ext) == 0){
                found = true;
                break;
            }
        }
        if(!found){
            supports_all_required_extensions = false;
            break;
        }
    }

    if(!supports_all_required_extensions){
        return false;
    }

    // Check the features availability
    auto features = device.template getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    >();

    bool supportsRequiredFeatures =
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;



    return supportsRequiredFeatures;
}


void Device::pickPhysicalDevice(vk::raii::Instance& instance){
    auto devices = instance.enumeratePhysicalDevices();

    if(devices.empty()){
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for(vk::raii::PhysicalDevice& device : devices){
        if(isDeviceSuitable(device)){
            physical_device = device;
            std::cout << "Picked device: "<< device.getProperties().deviceName << std::endl;
            break;
        }
    }

    if(physical_device == nullptr){
        throw std::runtime_error("failed to find GPU with all the necessary features");
    }
}


uint32_t Device::findQueueFamilies(){
    // Get all queue families available on the physical device
    std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();

    // find first queue family in the list whose queueFlag contains the eGraphics
    // eGraphics means it can submit graphics draw calls
    auto graphics_queue_family_property =
      std::find_if( queue_family_properties.begin(),
                    queue_family_properties.end(),
                    []( vk::QueueFamilyProperties const & qfp ) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; } );

    // return the index of the found queue family
    return static_cast<uint32_t>( std::distance( queue_family_properties.begin(), graphics_queue_family_property ) );
}

void Device::createLogicalDevice(){
    // find the index of the first queue fammily that supports graphics
    auto graphics_index = findQueueFamilies();

    // query for Vulkan 1.3 features
    vk::StructureChain<
    vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    > feature_chain{
        vk::PhysicalDeviceFeatures2{},                           // No core features for now
        vk::PhysicalDeviceVulkan13Features{ VK_TRUE },           // dynamicRendering = true
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{ VK_TRUE } // extendedDynamicState = true
    };

    // create a Device
    float queue_priority = 0.0f; // priority goes from 0.0 to 1.0, it is required even for only 1
    vk::DeviceQueueCreateInfo device_queue_create_info{
        {},                // flags (usually none)
        graphics_index,    // queue family index
        1,                 // number of queues
        &queue_priority    // pointer to priorities array
    };
    vk::DeviceCreateInfo device_create_info{
        {},                               // flags (usually none)
        1,                                // queueCreateInfoCount
        &device_queue_create_info,        // pQueueCreateInfos
        0,                                // enabledLayerCount (deprecated in Vulkan 1.0+)
        nullptr,                          // ppEnabledLayerNames (deprecated)
        static_cast<uint32_t>(device_extensions.size()), // enabledExtensionCount
        device_extensions.data(),         // ppEnabledExtensionNames
        nullptr,                          // pEnabledFeatures (null because we use pNext for feature chain)
        &feature_chain                    // pNext -> features chain
    };

    logical_device = vk::raii::Device(physical_device, device_create_info);
    graphics_queue = vk::raii::Queue( logical_device, graphics_index, 0 );
}

