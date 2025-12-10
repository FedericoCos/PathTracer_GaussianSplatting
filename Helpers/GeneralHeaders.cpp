#include "GeneralHeaders.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf-release/tiny_gltf.h"


// ------ Helper Functions

static const char* VmaResultToString(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        // add others if needed
        default: return "VkResult(unknown)";
    }
}

vk::raii::CommandBuffer beginSingleTimeCommands(vk::raii::CommandPool& command_pool, vk::raii::Device *logical_device){
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;

    vk::raii::CommandBuffer command_buffer = std::move(logical_device -> allocateCommandBuffers(alloc_info).front());

    vk::CommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    command_buffer.begin(command_buffer_begin_info);

    return command_buffer;
}

void endSingleTimeCommands(vk::raii::CommandBuffer &command_buffer, vk::raii::Queue& queue)
{
    command_buffer.end();

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &*command_buffer;
    queue.submit(submit_info, nullptr);
    queue.waitIdle();
    
}

vk::Format findSupportedFormat(vk::raii::PhysicalDevice &physical_device, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    for (const auto format : candidates){
        vk::FormatProperties props = physical_device.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features){
            return format;
        }
        if(tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features){
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

bool hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

vk::Format findDepthFormat(vk::raii::PhysicalDevice &physical_device)
{
    return findSupportedFormat(physical_device,
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

// ------ General Functions

void createBuffer(
    VmaAllocator& vma_allocator,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    AllocatedBuffer &allocated_buffer)
{
    // Prepare VMA alloc info
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

    // Decide flags by host-visible property
    if ((properties & vk::MemoryPropertyFlagBits::eHostVisible) != vk::MemoryPropertyFlags{}) {
        // Staging-like buffer: request mapped & sequential write host access
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    } else {
        // Device-only buffer: no special flags (not mapped)
        alloc_info.flags = 0;
    }

    // Fill VkBufferCreateInfo (use raw Vulkan struct via cast)
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;


    VkResult r = vmaCreateBuffer(vma_allocator, reinterpret_cast<VkBufferCreateInfo const*>(&buffer_info), 
            &alloc_info, &allocated_buffer.buffer, &allocated_buffer.allocation, &allocated_buffer.info);
    if (r != VK_SUCCESS) {
        std::stringstream ss;
        ss << "vmaCreateBuffer failed: " << VmaResultToString(r) << " (" << r << ")";
        throw std::runtime_error(ss.str());
    }
    allocated_buffer.p_allocator = &vma_allocator;
}

void copyBuffer(VkBuffer &src_buffer, VkBuffer &dst_buffer, vk::DeviceSize size, vk::raii::CommandPool &command_pool, vk::raii::Device *logical_device, vk::raii::Queue &queue)
{
    vk::raii::CommandBuffer command_copy_buffer = beginSingleTimeCommands(command_pool, logical_device);

    command_copy_buffer.copyBuffer(src_buffer, dst_buffer, vk::BufferCopy(0, 0, size));

    endSingleTimeCommands(command_copy_buffer, queue);
}

/**
 * Reads the values present in a buffer 
 * Used for importanceSampling and to save pointcloud info
 */
void readBuffer(vk::Buffer buffer, vk::DeviceSize size, void* dst_ptr, VmaAllocator &vma_allocator,
                vk::raii::Device *logical_device, vk::raii::CommandPool &command_pool,
                vk::raii::Queue &queue) {
    AllocatedBuffer staging_buffer;
    createBuffer(vma_allocator, size, vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                 staging_buffer);

    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(command_pool, logical_device);
    vk::BufferCopy copy_region(0, 0, size);
    cmd.copyBuffer(buffer, staging_buffer.buffer, copy_region);
    endSingleTimeCommands(cmd, queue);

    void* data;
    vmaMapMemory(vma_allocator, staging_buffer.allocation, &data);
    memcpy(dst_ptr, data, (size_t)size);
    vmaUnmapMemory(vma_allocator, staging_buffer.allocation);
}

void copyBufferToImage(const VkBuffer &buffer, VkImage &image, uint32_t width, uint32_t height, vk::raii::Device *logical_device, vk::raii::CommandPool &command_pool, vk::raii::Queue &queue)
{
    vk::raii::CommandBuffer command_buffer = beginSingleTimeCommands(command_pool, logical_device);
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};
    command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
    endSingleTimeCommands(command_buffer, queue);
}

void savePNG(const std::string& filename, const ImageReadbackData& data) {
    // Note: STBI expects R8G8B8A8 data (4 channels)
    int result = stbi_write_png(filename.c_str(), 
                                data.width, 
                                data.height, 
                                4, // 4 channels (RGBA)
                                data.data.data(), 
                                data.width * 4); // Stride

    if (result == 0) {
        std::cerr << "Error: Failed to save PNG file: " << filename << std::endl;
    } else {
        std::cout << "Successfully saved: " << filename << std::endl;
    }
}

void saveJPG(const std::string& filename, const ImageReadbackData& data, int quality) {
    // 4 channels (RGBA), width * 4 stride
    int result = stbi_write_jpg(filename.c_str(), 
                                data.width, 
                                data.height, 
                                4, 
                                data.data.data(), 
                                quality);

    if (result == 0) {
        std::cerr << "Error: Failed to save JPG file: " << filename << std::endl;
    } else {
        std::cout << "Successfully saved: " << filename << std::endl;
    }
}
