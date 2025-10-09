#include "torus.h"
#include "engine.h"

void Torus::createDescriptorSets(Engine &engine){
    // We assume all objects use the same layout for now.
    // In a more advanced engine, you might pass the specific layout this object needs.
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *engine.descriptor_set_layout);
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = engine.descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    // Allocate the descriptor sets for this object
    descriptor_sets = engine.logical_device.allocateDescriptorSets(alloc_info);

    // Update each descriptor set to point to the correct resources
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo buffer_info;
        buffer_info.buffer = engine.uniform_buffers[i].buffer;
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        std::array<vk::WriteDescriptorSet, 1> descriptor_writes = {};
        descriptor_writes[0].dstSet = descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        engine.logical_device.updateDescriptorSets(descriptor_writes, {});
    }
}