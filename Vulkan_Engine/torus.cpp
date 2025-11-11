#include "torus.h"
#include "engine.h"

void Torus::createMaterialDescriptorSets(Engine& engine) {
    // This implementation is for a transparent-only object (torus)
    // It only has one material.
    if (materials.empty()) {
        std::cout << "Torus has no materials, skipping descriptor set creation." << std::endl;
        return;
    }
    
    Material& material = materials[0]; // Assume only one material
    
    // We use the t_pipeline's layout (transparent pipeline)
    if (!t_pipeline || t_pipeline->descriptor_set_layout == nullptr) {
        throw std::runtime_error("Torus t_pipeline or descriptor_set_layout is not set!");
    }

    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *t_pipeline->descriptor_set_layout);
    
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = engine.descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    // Allocate the descriptor sets
    try {
         material.descriptor_sets = engine.logical_device.allocateDescriptorSets(alloc_info);
    } catch (vk::SystemError& err) {
        throw std::runtime_error(std::string("Failed to allocate descriptor sets for torus: ") + err.what());
    }

    // Write to the descriptor sets
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = engine.uniform_buffers[i].buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        vk::WriteDescriptorSet descriptor_write;
        
        // Binding 0: UBO
        descriptor_write.dstSet = material.descriptor_sets[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buffer_info;

        engine.logical_device.updateDescriptorSets(descriptor_write, {});
    }
}
