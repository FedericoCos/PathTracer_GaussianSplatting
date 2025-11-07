#include "device.h"
#include "engine.h"

std::vector<const char*> device_extensions = {
    vk::KHRSwapchainExtensionName, // extension required for presenting rendered images to the window
    vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName,
    vk::KHRCreateRenderpass2ExtensionName
};

bool Device::isDeviceSuitable(const vk::raii::PhysicalDevice &device, bool descrete)
{
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
    
    std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

    for(const auto& extension : available_device_extensions){
        required_extensions.erase(extension.extensionName);
    }

    if(!required_extensions.empty()){
        return false;
    }

    // Check the features availability
    auto features = device.template getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    >();

    bool supportsRequiredFeatures =
        features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
        features.template get<vk::PhysicalDeviceFeatures2>().features.independentBlend &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
        features.template get<vk::PhysicalDeviceFeatures2>().features.shaderStorageImageMultisample &&
        features.template get<vk::PhysicalDeviceFeatures2>().features.sampleRateShading &&
        features.template get<vk::PhysicalDeviceVulkan11Features>().multiview;;



    return supportsRequiredFeatures;
}

vk::raii::PhysicalDevice Device::pickPhysicalDevice(const Engine& engine){
    auto devices = engine.instance.enumeratePhysicalDevices();

    if(devices.empty()){
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    // First, we look for a discrete GPU, generally much better than integrated
    for(const vk::raii::PhysicalDevice& device : devices){
        std::cout << "Checking device: " << device.getProperties().deviceName << std::endl;
        if(isDeviceSuitable(device, true)){
            std::cout << "Picked device: "<< device.getProperties().deviceName << std::endl;
            return device;
        }
    }

    // If Discrete graphic not found, we look for an integrated one
    for(const vk::raii::PhysicalDevice& device : devices){
        std::cout << "Checking device: " << device.getProperties().deviceName << std::endl;
        if(isDeviceSuitable(device, false)){
            std::cout << "Picked device: "<< device.getProperties().deviceName << std::endl;
            return device;
        }
    }

    throw std::runtime_error("failed to find GPU with all the necessary features");
}


QueueFamilyIndices Device::findQueueFamilies(const Engine& engine){
    QueueFamilyIndices indices;

    // Get all queue families available on the physical device
    std::vector<vk::QueueFamilyProperties> queue_family_properties = engine.physical_device.getQueueFamilyProperties();

    uint32_t i = 0;
    for(const auto& queue_family : queue_family_properties){
        // Find a graphics queue
        if(queue_family.queueFlags & vk::QueueFlagBits::eGraphics){
            indices.graphics_family = i;
        }

        // Find a dedicated transfer queue (one that supports transfer but not graphics)
        // usually faster for transfers because of DMA engine
        if((queue_family.queueFlags & vk::QueueFlagBits::eTransfer) &&
            !(queue_family.queueFlags & vk::QueueFlagBits::eGraphics)){
            indices.transfer_family = i;
        }

        // Find a presentation queue
        if(engine.physical_device.getSurfaceSupportKHR(i, *engine.surface)){
            indices.present_family = i;
        }

        if(indices.isComplete()){
            break;
        }
        i++;
    }

    // Falling back to 
    if(!indices.transfer_family.has_value() && indices.graphics_family.has_value()){
        indices.transfer_family = indices.graphics_family;
    }
    return indices;
}

vk::raii::Device Device::createLogicalDevice(const Engine &engine, QueueFamilyIndices &indices){
    // find the index of the first queue fammily that supports graphics
    indices = findQueueFamilies(engine);
    std::set<uint32_t> unique_queue_families = {
        indices.graphics_family.value(),
        indices.present_family.value(),
        indices.transfer_family.value()
    };

    vk::PhysicalDeviceVulkan11Features vulkan11features;
    vulkan11features.multiview = true;

    // query for Vulkan 1.3 features
    vk::PhysicalDeviceVulkan13Features vulkan13features;
    vulkan13features.synchronization2 = true;
    vulkan13features.dynamicRendering = true;

    vk::PhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.features.samplerAnisotropy = true;
    deviceFeatures2.features.independentBlend = true;
    deviceFeatures2.features.fragmentStoresAndAtomics = vk::True; 
    deviceFeatures2.features.shaderStorageImageMultisample = vk::True;
    deviceFeatures2.features.sampleRateShading = vk::True;
    

    vk::StructureChain<
    vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan11Features,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
    vk::PhysicalDeviceBufferDeviceAddressFeatures
    > feature_chain{
        deviceFeatures2,
        vulkan11features,
        vulkan13features,           // dynamicRendering = true
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{ VK_TRUE }, // extendedDynamicState = true
        vk::PhysicalDeviceBufferDeviceAddressFeatures { VK_TRUE }
    };

    // create a Device
    float queue_priority = 1.0f; // priority goes from 0.0 to 1.0, it is required even for only 1
    std::vector<vk::DeviceQueueCreateInfo> device_queue_create_infos;
    for(const uint32_t index : unique_queue_families){
        device_queue_create_infos.push_back({
            {}, // flags (usually none)
            index, // queue family index
            1, // number of queues from this family
            &queue_priority // priority
        });
    };


    vk::DeviceCreateInfo device_create_info{
        {},                               // flags (usually none)
        static_cast<uint32_t>(device_queue_create_infos.size()),                                // queueCreateInfoCount
        device_queue_create_infos.data(),        // pQueueCreateInfos
        0,                                // enabledLayerCount (deprecated in Vulkan 1.0+)
        nullptr,                          // ppEnabledLayerNames (deprecated)
        static_cast<uint32_t>(device_extensions.size()), // enabledExtensionCount
        device_extensions.data(),         // ppEnabledExtensionNames
        nullptr,                          // pEnabledFeatures (null because we use pNext for feature chain)
        &feature_chain.get<vk::PhysicalDeviceFeatures2>()    // pNext -> features chain
    };

    return std::move(vk::raii::Device(engine.physical_device, device_create_info));
}

vk::raii::Queue Device::getQueue(const Engine &engine, const uint32_t &index){
    return std::move(vk::raii::Queue(engine.logical_device, index, 0));
}

