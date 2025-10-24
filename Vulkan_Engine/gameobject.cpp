#include "gameobject.h"
#include "engine.h"
#include "image.h"


// --- HELPER FUNCTION (add this to the top of gameobject.cpp) ---
// Creates a 1x1 white texture to use as a fallback.
void createDefaultTexture(Engine& engine, AllocatedImage& texture) {
    unsigned char whitePixel[] = {255, 255, 255, 255};
    vk::DeviceSize image_size = 4;

    AllocatedBuffer staging_buffer;
    createBuffer(engine.vma_allocator, image_size,
                vk::BufferUsageFlagBits::eTransferSrc, 
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);
    
    void *data;
    vmaMapMemory(engine.vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, whitePixel, image_size);
    vmaUnmapMemory(engine.vma_allocator, staging_buffer.allocation);

    texture = Image::createImage(1, 1, 1, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::MemoryPropertyFlagBits::eDeviceLocal, engine);
    
    texture.image_view = Image::createImageView(texture, engine);

    Image::transitionImageLayout(texture.image, 1, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, engine);
    copyBufferToImage(staging_buffer.buffer, texture.image, 1, 1, &engine.logical_device, engine.command_pool_transfer, engine.transfer_queue);
    Image::transitionImageLayout(texture.image, 1, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, engine);
}


bool Gameobject::inputUpdate(InputState &input, float &dtime)
{
    return false;
}

void Gameobject::loadModel(std::string m_path, Engine &engine)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // --- File Loading ---
    bool ret = false;
    std::string ext = m_path.substr(m_path.find_last_of('.') + 1);

    if (ext == "gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_path);
    } else if (ext == "glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, m_path);
    } else {
        throw std::runtime_error("Failed to load glTF: Unknown file extension for " + m_path);
    }

    if (!warn.empty()) std::cout << "glTF warning: " << warn << std::endl;
    if (!err.empty()) std::cout << "glTF error: " << err << std::endl;
    if (!ret) throw std::runtime_error("Failed to load glTF model: " + m_path);

    // --- Clear previous data ---
    vertices.clear();
    indices.clear();
    textures.clear();
    materials.clear();
    primitives.clear();

    std::string base_dir = m_path.substr(0, m_path.find_last_of('/') + 1);

    // --- 1. Load Textures ---
    std::map<int, vk::Format> image_formats;

    // Helper lambda to safely get the image source index from a texture index
    auto getImageSourceIndex = [&](int texIdx) -> int {
        if (texIdx >= 0 && texIdx < model.textures.size()) {
            return model.textures[texIdx].source;
        }
        return -1;
    };

    for (const auto& mat : model.materials) {
        int sourceIdx = -1;

        // --- sRGB (Color) Textures ---
        sourceIdx = getImageSourceIndex(mat.pbrMetallicRoughness.baseColorTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Srgb;

        sourceIdx = getImageSourceIndex(mat.emissiveTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Srgb;

        // --- Unorm (Linear Data) Textures ---
        sourceIdx = getImageSourceIndex(mat.normalTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;

        sourceIdx = getImageSourceIndex(mat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;

        sourceIdx = getImageSourceIndex(mat.occlusionTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;

        // Check for clearcoat extensions (this model uses them )
        if (mat.extensions.count("KHR_materials_clearcoat")) {
            const auto& clearcoat = mat.extensions.at("KHR_materials_clearcoat");
            if (clearcoat.Has("clearcoatTexture")) {
                sourceIdx = getImageSourceIndex(clearcoat.Get("clearcoatTexture").Get("index").Get<int>());
                if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
            }
            if (clearcoat.Has("clearcoatRoughnessTexture")) {
                sourceIdx = getImageSourceIndex(clearcoat.Get("clearcoatRoughnessTexture").Get("index").Get<int>());
                if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
            }
        }

        
    }

    textures.emplace_back();
    createDefaultTexture(engine, textures[0]); // Default white texture

    int image_index = 0;
    for (const auto& image : model.images) {
        std::string texture_path = base_dir + image.uri;
        vk::Format format = vk::Format::eR8G8B8A8Srgb;
        if (image_formats.contains(image_index)) {
            format = image_formats[image_index];
        }

        textures.push_back(Image::createTextureImage(engine, texture_path.c_str(), format));
        
        image_index++;
    }

    default_sampler = Image::createTextureSampler(engine.physical_device, &engine.logical_device, 1);

    // --- 2. Load Materials ---
    for (const auto& mat : model.materials) {
        Material newMaterial;
        const auto& pbr = mat.pbrMetallicRoughness;
        const auto& bcf = pbr.baseColorFactor;
        newMaterial.base_color_factor = glm::vec4(bcf[0], bcf[1], bcf[2], bcf[3]);
        newMaterial.metallic_factor = static_cast<float>(pbr.metallicFactor);
        newMaterial.roughness_factor = static_cast<float>(pbr.roughnessFactor);

        const auto& ef = mat.emissiveFactor;
        newMaterial.emissive_factor = glm::vec3(ef[0], ef[1], ef[2]);
        if (mat.extensions.count("KHR_materials_emissive_strength")) {
            const auto& strength_ext = mat.extensions.at("KHR_materials_emissive_strength");
            if (strength_ext.Has("emissiveStrength")) {
                float strength = static_cast<float>(strength_ext.Get("emissiveStrength").Get<double>());
                newMaterial.emissive_factor *= strength;
            }
        }

        auto getTexIndex = [&](int texIdx) -> int {
            if (texIdx >= 0 && texIdx < model.textures.size())
                return model.textures[texIdx].source + 1;
            return 0;
        };

        newMaterial.albedo_texture_index = getTexIndex(pbr.baseColorTexture.index);
        newMaterial.normal_texture_index = getTexIndex(mat.normalTexture.index);
        newMaterial.metallic_roughness_texture_index = getTexIndex(pbr.metallicRoughnessTexture.index);

        newMaterial.occlusion_texture_index = getTexIndex(mat.occlusionTexture.index); // <-- NEW
        newMaterial.occlusion_strength = static_cast<float>(mat.occlusionTexture.strength); // <-- NEW
        
        newMaterial.emissive_texture_index = getTexIndex(mat.emissiveTexture.index); // <-- NEW

        newMaterial.specular_color_factor = glm::vec3(1.0f);
        newMaterial.specular_factor = 0.5f; // Default for IOR 1.5

        if (mat.extensions.count("KHR_materials_specular")) {
            const auto& spec_ext = mat.extensions.at("KHR_materials_specular");
            
            if (spec_ext.Has("specularFactor")) {
                newMaterial.specular_factor = static_cast<float>(spec_ext.Get("specularFactor").Get<double>());
            }
            
            if (spec_ext.Has("specularColorFactor")) {
                // This is the corrected part:
                const auto& colorValue = spec_ext.Get("specularColorFactor");
                if (colorValue.IsArray() && colorValue.ArrayLen() >= 3) {
                    newMaterial.specular_color_factor = glm::vec3(
                        colorValue.Get(0).Get<double>(),
                        colorValue.Get(1).Get<double>(),
                        colorValue.Get(2).Get<double>()
                    );
                }
            }
        }

        if (mat.extensions.count("KHR_materials_clearcoat")) {
            const auto& cc_ext = mat.extensions.at("KHR_materials_clearcoat");

            if (cc_ext.Has("clearcoatFactor")) {
                newMaterial.clearcoat_factor = static_cast<float>(cc_ext.Get("clearcoatFactor").Get<double>());
            }
            if (cc_ext.Has("clearcoatRoughnessFactor")) {
                newMaterial.clearcoat_roughness_factor = static_cast<float>(cc_ext.Get("clearcoatRoughnessFactor").Get<double>());
            }
            if (cc_ext.Has("clearcoatTexture")) {
                newMaterial.clearcoat_texture_index = getTexIndex(cc_ext.Get("clearcoatTexture").Get("index").Get<int>());
            }
            if (cc_ext.Has("clearcoatRoughnessTexture")) {
                newMaterial.clearcoat_roughness_texture_index = getTexIndex(cc_ext.Get("clearcoatRoughnessTexture").Get("index").Get<int>());
            }
        }

        materials.push_back(std::move(newMaterial));
    }
    if (materials.empty()) {
        materials.emplace_back();
        materials.back().albedo_texture_index = 0;
    }

    std::unordered_map<Vertex, uint32_t> unique_vertices{};

    // --- Node Transform Helper ---
    auto getNodeTransform = [&](const tinygltf::Node& node) -> glm::mat4 {
        glm::mat4 matrix(1.0f);
        if (node.matrix.size() == 16) {
            matrix = glm::make_mat4(node.matrix.data());
        } else {
            glm::vec3 translation(0.0f);
            if (node.translation.size() == 3)
                translation = glm::make_vec3(node.translation.data());

            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if (node.rotation.size() == 4)
                rotation = glm::make_quat(node.rotation.data());

            glm::vec3 scale(1.0f);
            if (node.scale.size() == 3)
                scale = glm::make_vec3(node.scale.data());

            matrix = glm::translate(glm::mat4(1.0f), translation)
                   * glm::mat4_cast(rotation)
                   * glm::scale(glm::mat4(1.0f), scale);
        }
        return matrix;
    };

    // --- Primitive Loader ---
    auto loadPrimitive = [&](const tinygltf::Primitive& primitive, const tinygltf::Model& model, const glm::mat4& transform) {
        Primitive newPrimitive;
        newPrimitive.material_index = (primitive.material >= 0) ? primitive.material : 0;
        newPrimitive.first_index = static_cast<uint32_t>(indices.size());

        const tinygltf::Accessor& index_accessor = model.accessors[primitive.indices];
        newPrimitive.index_count = static_cast<uint32_t>(index_accessor.count);

        const tinygltf::Accessor& pos_accessor = model.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::BufferView& pos_buffer_view = model.bufferViews[pos_accessor.bufferView];
        const unsigned char* pos_data = &model.buffers[pos_buffer_view.buffer].data[pos_buffer_view.byteOffset + pos_accessor.byteOffset];
        size_t pos_stride = pos_buffer_view.byteStride ? pos_buffer_view.byteStride : sizeof(glm::vec3);

        bool has_normals = primitive.attributes.count("NORMAL");
        const unsigned char* normal_data = nullptr;
        size_t normal_stride = 0;
        if (has_normals) {
            const tinygltf::Accessor& normal_accessor = model.accessors[primitive.attributes.at("NORMAL")];
            const tinygltf::BufferView& normal_buffer_view = model.bufferViews[normal_accessor.bufferView];
            normal_data = &model.buffers[normal_buffer_view.buffer].data[normal_buffer_view.byteOffset + normal_accessor.byteOffset];
            normal_stride = normal_buffer_view.byteStride ? normal_buffer_view.byteStride : sizeof(glm::vec3);
        }

        bool has_tangents = primitive.attributes.count("TANGENT");
        const unsigned char* tangent_data = nullptr;
        size_t tangent_stride = 0;
        if (has_tangents) {
            const tinygltf::Accessor& tangent_accessor = model.accessors[primitive.attributes.at("TANGENT")];
            const tinygltf::BufferView& tangent_buffer_view = model.bufferViews[tangent_accessor.bufferView];
            tangent_data = &model.buffers[tangent_buffer_view.buffer].data[tangent_buffer_view.byteOffset + tangent_accessor.byteOffset];
            tangent_stride = tangent_buffer_view.byteStride ? tangent_buffer_view.byteStride : sizeof(glm::vec4);
        }

        bool has_tex_coords = primitive.attributes.count("TEXCOORD_0");
        const unsigned char* tex_coord_data = nullptr;
        size_t tex_coord_stride = 0;
        if (has_tex_coords) {
            const tinygltf::Accessor& tex_coord_accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            const tinygltf::BufferView& tex_coord_buffer_view = model.bufferViews[tex_coord_accessor.bufferView];
            tex_coord_data = &model.buffers[tex_coord_buffer_view.buffer].data[tex_coord_buffer_view.byteOffset + tex_coord_accessor.byteOffset];
            tex_coord_stride = tex_coord_buffer_view.byteStride ? tex_coord_buffer_view.byteStride : sizeof(glm::vec2);
        }

        bool has_tex_coords_1 = primitive.attributes.count("TEXCOORD_1");
        const unsigned char* tex_coord_data_1 = nullptr;
        size_t tex_coord_stride_1 = 0;
        if (has_tex_coords_1) {
            const tinygltf::Accessor& tex_coord_accessor = model.accessors[primitive.attributes.at("TEXCOORD_1")];
            const tinygltf::BufferView& tex_coord_buffer_view = model.bufferViews[tex_coord_accessor.bufferView];
            tex_coord_data_1 = &model.buffers[tex_coord_buffer_view.buffer].data[tex_coord_buffer_view.byteOffset + tex_coord_accessor.byteOffset];
            tex_coord_stride_1 = tex_coord_buffer_view.byteStride ? tex_coord_buffer_view.byteStride : sizeof(glm::vec2);
        }

        const tinygltf::BufferView& index_buffer_view = model.bufferViews[index_accessor.bufferView];
        const unsigned char* index_data = &model.buffers[index_buffer_view.buffer].data[index_buffer_view.byteOffset + index_accessor.byteOffset];

        for (size_t i = 0; i < index_accessor.count; i++) {
            uint32_t orig_idx = 0;
            switch (index_accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    orig_idx = reinterpret_cast<const uint8_t*>(index_data)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    orig_idx = reinterpret_cast<const uint16_t*>(index_data)[i];
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    orig_idx = reinterpret_cast<const uint32_t*>(index_data)[i];
                    break;
            }

            Vertex vertex{};
            vertex.color = {1.f, 1.f, 1.f};
            vertex.pos = *reinterpret_cast<const glm::vec3*>(pos_data + (orig_idx * pos_stride));
            vertex.normal = has_normals ? *reinterpret_cast<const glm::vec3*>(normal_data + (orig_idx * normal_stride))
                                        : glm::vec3(0.0f, 0.0f, 1.0f);
            vertex.tangent = has_tangents ? *reinterpret_cast<const glm::vec4*>(tangent_data + (orig_idx * tangent_stride))
                                          : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

            if (has_tex_coords) {
                glm::vec2 raw_tex = *reinterpret_cast<const glm::vec2*>(tex_coord_data + (orig_idx * tex_coord_stride));
                vertex.tex_coord = {raw_tex.x, 1.0f - raw_tex.y};
            }
            if (has_tex_coords_1) {
                glm::vec2 raw_tex = *reinterpret_cast<const glm::vec2*>(tex_coord_data_1 + (orig_idx * tex_coord_stride_1));
                vertex.tex_coord_1 = {raw_tex.x, 1.0f - raw_tex.y};
            } else if (has_tex_coords) {
                vertex.tex_coord_1 = vertex.tex_coord;
            }

            // Apply node transform
            vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1.0f));
            vertex.normal = glm::normalize(glm::mat3(transform) * vertex.normal);

            if (!unique_vertices.contains(vertex)) {
                unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(unique_vertices[vertex]);
        }

        primitives.push_back(newPrimitive);
    };

    // --- Recursive Node Traversal ---
    std::function<void(int, glm::mat4)> processNode;
    processNode = [&](int nodeIndex, glm::mat4 parentTransform) {
        const tinygltf::Node& node = model.nodes[nodeIndex];
        glm::mat4 localTransform = parentTransform * getNodeTransform(node);

        if (node.mesh >= 0) {
            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (const auto& primitive : mesh.primitives)
                loadPrimitive(primitive, model, localTransform);
        }

        for (int child : node.children)
            processNode(child, localTransform);
    };

    // --- Start from Default Scene ---
    if (model.scenes.empty()) throw std::runtime_error("glTF has no scenes!");

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    for (int nodeIndex : model.scenes[sceneIndex].nodes)
        processNode(nodeIndex, glm::mat4(1.0f));
}


void Gameobject::createMaterialDescriptorSets(Engine& engine) {
    
    // Get the texture view for a given index, falling back to default
    auto getImageView = [&](int index) -> vk::raii::ImageView& {
        if (index >= 0 && index < textures.size() && textures[index].image_view != nullptr) {
            return textures[index].image_view;
        }
        return textures[0].image_view; // Return default white texture
    };

    for (auto& material : materials) {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *pipeline->descriptor_set_layout);
        vk::DescriptorSetAllocateInfo alloc_info;
        alloc_info.descriptorPool = engine.descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        alloc_info.pSetLayouts = layouts.data();

        material.descriptor_sets = engine.logical_device.allocateDescriptorSets(alloc_info);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::DescriptorBufferInfo buffer_info;
            buffer_info.buffer = engine.uniform_buffers[i].buffer;
            buffer_info.offset = 0;
            buffer_info.range = sizeof(UniformBufferObject);

            vk::DescriptorImageInfo albedo_info = {};
            albedo_info.sampler = *default_sampler;
            albedo_info.imageView = *getImageView(material.albedo_texture_index);
            albedo_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            
            vk::DescriptorImageInfo normal_info = {};
            normal_info.sampler = *default_sampler;
            // Use default texture if no normal map, or a default 1x1 normal map
            normal_info.imageView = *getImageView(material.normal_texture_index); 
            normal_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::DescriptorImageInfo met_rough_info = {};
            met_rough_info.sampler = *default_sampler;
            met_rough_info.imageView = *getImageView(material.metallic_roughness_texture_index);
            met_rough_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::DescriptorImageInfo ao_info = {}; // <-- NEW
            ao_info.sampler = *default_sampler;
            ao_info.imageView = *getImageView(material.occlusion_texture_index);
            ao_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::DescriptorImageInfo emissive_info = {}; // <-- NEW
            emissive_info.sampler = *default_sampler;
            emissive_info.imageView = *getImageView(material.emissive_texture_index);
            emissive_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;


            vk::DescriptorImageInfo clearcoat_info = {};
            clearcoat_info.sampler = *default_sampler;
            clearcoat_info.imageView = *getImageView(material.clearcoat_texture_index);
            clearcoat_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::DescriptorImageInfo clearcoat_roughness_info = {};
            clearcoat_roughness_info.sampler = *default_sampler;
            clearcoat_roughness_info.imageView = *getImageView(material.clearcoat_roughness_texture_index);
            clearcoat_roughness_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            std::array<vk::WriteDescriptorSet, 8> descriptor_writes = {};
            
            // Binding 0: UBO
            descriptor_writes[0].dstSet = material.descriptor_sets[i];
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo = &buffer_info;

            // Binding 1: Albedo
            descriptor_writes[1].dstSet = material.descriptor_sets[i];
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pImageInfo = &albedo_info;

            // Binding 2: Normal
            descriptor_writes[2].dstSet = material.descriptor_sets[i];
            descriptor_writes[2].dstBinding = 2;
            descriptor_writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[2].descriptorCount = 1;
            descriptor_writes[2].pImageInfo = &normal_info;

            // Binding 3: Metallic/Roughness
            descriptor_writes[3].dstSet = material.descriptor_sets[i];
            descriptor_writes[3].dstBinding = 3;
            descriptor_writes[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[3].descriptorCount = 1;
            descriptor_writes[3].pImageInfo = &met_rough_info;

            // Binding 4: Ambient Occlusion
            descriptor_writes[4].dstSet = material.descriptor_sets[i];
            descriptor_writes[4].dstBinding = 4;
            descriptor_writes[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[4].descriptorCount = 1;
            descriptor_writes[4].pImageInfo = &ao_info;

            // Binding 5: Emissive
            descriptor_writes[5].dstSet = material.descriptor_sets[i];
            descriptor_writes[5].dstBinding = 5;
            descriptor_writes[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[5].descriptorCount = 1;
            descriptor_writes[5].pImageInfo = &emissive_info;

            // Binding 6: Clearcoat
            descriptor_writes[6].dstSet = material.descriptor_sets[i];
            descriptor_writes[6].dstBinding = 6;
            descriptor_writes[6].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[6].descriptorCount = 1;
            descriptor_writes[6].pImageInfo = &clearcoat_info;

            // Binding 7: Clearcoat Roughness
            descriptor_writes[7].dstSet = material.descriptor_sets[i];
            descriptor_writes[7].dstBinding = 7;
            descriptor_writes[7].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            descriptor_writes[7].descriptorCount = 1;
            descriptor_writes[7].pImageInfo = &clearcoat_roughness_info;

            engine.logical_device.updateDescriptorSets(descriptor_writes, {});
        }
    }
}
