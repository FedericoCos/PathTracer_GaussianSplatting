#include "device.h"

bool Device::isDeviceSuitable(vk::raii::PhysicalDevice& device, bool descrete){
    auto device_properties = device.getProperties();
    auto device_features = device.getFeatures();

    // Checking if supported Vulkan is at least 1.3
    if(device_properties.apiVersion < VK_API_VERSION_1_3){
        return false;
    }

    // Picking only Discrete GPU
    if(!(device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu || !descrete) ||
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
        features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;



    return supportsRequiredFeatures;
}


void Device::pickPhysicalDevice(vk::raii::Instance& instance){
    auto devices = instance.enumeratePhysicalDevices();

    if(devices.empty()){
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for(vk::raii::PhysicalDevice& device : devices){
        std::cout << "Checking device: " << device.getProperties().deviceName << std::endl;
        if(isDeviceSuitable(device, true)){
            physical_device = device;
            std::cout << "Picked device: "<< device.getProperties().deviceName << std::endl;
            break;
        }
    }

    if(physical_device == nullptr){
        for(vk::raii::PhysicalDevice& device : devices){
            std::cout << "Checking device: " << device.getProperties().deviceName << std::endl;
            if(isDeviceSuitable(device, false)){
                physical_device = device;
                std::cout << "Picked device: "<< device.getProperties().deviceName << std::endl;
                break;
            }
        }
    }

    if(physical_device == nullptr){
        throw std::runtime_error("failed to find GPU with all the necessary features");
    }
}


uint32_t Device::findQueueFamilies(vk::raii::SurfaceKHR& surface){
    // Get all queue families available on the physical device
    std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();

    // find first queue family in the list whose queueFlag contains the eGraphics
    // eGraphics means it can submit graphics draw calls
    for(uint32_t i = 0; i < queue_family_properties.size(); i++){
        if((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && 
                physical_device.getSurfaceSupportKHR(i, surface)){
                    return i;
                }
    }

    // return the index of the found queue family
    throw std::runtime_error("There is not a queu with both graphic and presentation");
    return -1;
}

void Device::createLogicalDevice(vk::raii::SurfaceKHR& surface){
    // find the index of the first queue fammily that supports graphics
    graphics_index = findQueueFamilies(surface);

    // query for Vulkan 1.3 features
    vk::PhysicalDeviceVulkan13Features vulkan13features;
    vulkan13features.synchronization2 = true;
    vulkan13features.dynamicRendering = true;

    vk::PhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.features.samplerAnisotropy = true;

    vk::StructureChain<
    vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
    vk::PhysicalDeviceBufferDeviceAddressFeatures
    > feature_chain{
        deviceFeatures2,
        vulkan13features,           // dynamicRendering = true
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{ VK_TRUE }, // extendedDynamicState = true
        vk::PhysicalDeviceBufferDeviceAddressFeatures { VK_TRUE }
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
        &feature_chain.get<vk::PhysicalDeviceFeatures2>()    // pNext -> features chain
    };

    logical_device = vk::raii::Device(physical_device, device_create_info);
    graphics_presentation_queue = vk::raii::Queue( logical_device, graphics_index, 0 );
}

