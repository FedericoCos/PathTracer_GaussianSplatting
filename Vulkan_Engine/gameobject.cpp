#include "gameobject.h"
#include "engine.h"
#include "image.h"

bool Gameobject::inputUpdate(InputState &input, float &dtime)
{
    return false;
}

void Gameobject::loadModel(std::string m_path, std::string t_path, Engine &engine)
{
    model_path = m_path;
    texture_path = t_path;

    // TEXTURE
    texture = Image::createTextureImage(engine, texture_path.c_str());
    texture_sampler = Image::createTextureSampler(engine.physical_device, &engine.logical_device, texture.mip_levels);

    //MODEL
    tinyobj::attrib_t attrib; // COntains all the positions, normals and texture coord
    std::vector<tinyobj::shape_t> shapes; // Contains all the separate objects and their faces
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str())){
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> unique_vertices{};

    for(const auto& shape : shapes){
        for(const auto& index : shape.mesh.indices){
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.tex_coord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.f, 1.f, 1.f};

            if(unique_vertices.count(vertex) == 0){
                unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(unique_vertices[vertex]);
        }
    }
}

void Gameobject::createDescriptorSets(Engine& engine) {
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

        vk::DescriptorImageInfo image_info = {};
        image_info.sampler = texture_sampler;
        image_info.imageView = texture.image_view;
        image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {};
        descriptor_writes[0].dstSet = descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        descriptor_writes[1].dstSet = descriptor_sets[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;

        engine.logical_device.updateDescriptorSets(descriptor_writes, {});
    }
}
