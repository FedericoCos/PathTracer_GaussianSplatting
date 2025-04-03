// Code for descriptors
#pragma once

#include "vk_types.h"

/**
 * Structure that represents a growable collection of Descriptor Pool
 */
struct DescriptorAllocatorGrowable{
public:
    struct PoolSizeRatio{
        VkDescriptorType type;
        float ratio;
    };

    void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRations);
    void clear_pools(VkDevice device);
    void destroy_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void * pNext = nullptr);

private:
    VkDescriptorPool get_pool(VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32_t setsPerPool;

};


/**
 * A dynamic buiulder for a Descriptor layout
 */
struct DescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice, VkShaderStageFlags shaderStages, void * pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};





/**
 * A structure that represents a Dynamic descriptors for faster update frame by frame
 */
struct DescriptorWriter
{
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};

