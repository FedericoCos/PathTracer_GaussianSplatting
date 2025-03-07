// Contains descriptor set abstractions

#include "vk_types.h"

struct DescriptorLayoutBuilder{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void * pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator{
    /**
     * Descriptor allocation happens through VkDescriptorPool. Those are objects that need to be 
     * pre-initialized with some size and types of descriptors for it. Think of it like a 
     * memory allocator for some specific descriptors. Its possible to have 1 very big
     * descriptor pool that handles the entire engine, but that means we need to know
     * what descriptors we will be using for everything ahead of time. We will have
     * multiple descriptor pools for different parts of the project, and try to be more accurate with them
     */
    struct PoolSizeRatio{
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolratios);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};