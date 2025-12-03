#include "gameobject.h"
#include "engine.h"
#include "image.h"
#include <map>
#include <stack> 
#include <utility>
#include <typeinfo>

// Helper: Get the first value from an animation accessor
template<typename T>
T getDataValue(const tinygltf::Model& model, int accessorIndex, int index) {
    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    
    // Calculate stride (if 0, implies tightly packed) -> fallback to size of T
    int stride = accessor.ByteStride(bufferView);
    if (stride == 0) stride = static_cast<int>(sizeof(T));
    
    const unsigned char* data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset + (index * stride);
    return *reinterpret_cast<const T*>(data);
}

// Calculate the global transform of every node in the hierarchy at frame 0
void computeGlobalNodeTransforms(const tinygltf::Model& model, 
                                 std::vector<glm::mat4>& node_globals) {
    node_globals.resize(model.nodes.size(), glm::mat4(1.0f));

    // Structure to hold local TRS overrides from animation
    struct TRS { 
        glm::vec3 t; glm::quat r; glm::vec3 s; 
        bool has_t=false, has_r=false, has_s=false;
    };
    std::map<int, TRS> animated_nodes;

    // 1. Extract Animation Data (Frame 0)
    if (!model.animations.empty()) {
        const auto& anim = model.animations[0]; // Use first animation
        for (const auto& channel : anim.channels) {
            int node = channel.target_node;
            int samplerIdx = channel.sampler;
            int output_acc = anim.samplers[samplerIdx].output;

            if (channel.target_path == "translation") {
                animated_nodes[node].t = getDataValue<glm::vec3>(model, output_acc, 0);
                animated_nodes[node].has_t = true;
            } else if (channel.target_path == "rotation") {
                // GLTF Quat is [x, y, z, w]
                // GLM Quat constructor is (w, x, y, z)
                glm::vec4 q = getDataValue<glm::vec4>(model, output_acc, 0);
                animated_nodes[node].r = glm::quat(q.w, q.x, q.y, q.z); 
                animated_nodes[node].has_r = true;
            } else if (channel.target_path == "scale") {
                animated_nodes[node].s = getDataValue<glm::vec3>(model, output_acc, 0);
                animated_nodes[node].has_s = true;
            }
        }
    }

    // 2. Recursive Hierarchy Traversal
    std::function<void(int, glm::mat4)> traverse = [&](int nodeIdx, glm::mat4 parentMat) {
        const auto& node = model.nodes[nodeIdx];
        glm::mat4 localMat = glm::mat4(1.0f);

        // A node can have a Matrix OR TRS. Animation overrides specific TRS components.
        if (node.matrix.size() == 16) {
            // If there's an animation on a matrix node, it's technically undefined behavior in GLTF 2.0
            // but we usually treat it as decomposable. For safety, just use the matrix if no anim.
            if (animated_nodes.count(nodeIdx) == 0) {
                // Convert double[] to float mat4
                float m_float[16];
                for(int i=0; i<16; ++i) m_float[i] = static_cast<float>(node.matrix[i]);
                localMat = glm::make_mat4(m_float);
            } else {
                // Fallback: If matrix node IS animated, we must decompose (complex), 
                // OR assume the animation provides full TRS. Let's assume full TRS override or defaults.
                glm::vec3 t(0.0f); glm::quat r(1.0f, 0.0f, 0.0f, 0.0f); glm::vec3 s(1.0f);
                // (Advanced decomposition logic omitted for brevity, assuming standard TRS usage)
            }
        }
        else {
            glm::vec3 t = glm::vec3(0.0f);
            glm::quat r = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 s = glm::vec3(1.0f);
            if(node.translation.size() == 3){
                t = glm::make_vec3(node.translation.data());
            }
            if(node.rotation.size() == 4){
                r = glm::make_quat(node.rotation.data());
            }
            if(node.scale.size() == 3){
                s = glm::make_vec3(node.scale.data());
            }

            // Apply Animation Overrides
            if (animated_nodes.count(nodeIdx)) {
                const auto& anim = animated_nodes[nodeIdx];
                if (anim.has_t) t = anim.t;
                if (anim.has_r) r = anim.r;
                if (anim.has_s) s = anim.s;
            }

            localMat = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
        }

        glm::mat4 globalMat = parentMat * localMat;
        node_globals[nodeIdx] = globalMat;

        for (int child : node.children) {
            traverse(child, globalMat);
        }
    };

    // Start traversal
    const auto& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
    for (int node : scene.nodes) {
        traverse(node, glm::mat4(1.0f));
    }
}


void Gameobject::createDefaultTexture(Engine& engine, AllocatedImage& texture, glm::vec4 color) {
    unsigned char colored_pixel[] = {
        static_cast<unsigned char>(color.x), 
        static_cast<unsigned char>(color.y), 
        static_cast<unsigned char>(color.z), 
        static_cast<unsigned char>(color.w)
    };
    vk::DeviceSize image_size = 4;

    AllocatedBuffer staging_buffer;
    createBuffer(engine.vma_allocator, image_size,
                vk::BufferUsageFlagBits::eTransferSrc, 
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                staging_buffer);
    
    void *data;
    vmaMapMemory(engine.vma_allocator, staging_buffer.allocation, &data);
    memcpy(data, colored_pixel, image_size);
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

    // Clear previous data
    vertices.clear();
    indices.clear();
    textures.clear();
    materials.clear();
    o_primitives.clear();

    // 1. Calculate global transforms for all nodes (Frame 0)
    std::vector<glm::mat4> node_globals;
    computeGlobalNodeTransforms(model, node_globals);

    // 2. Prepare Skinning Matrices
    skin_joint_matrices.clear();
    if (!model.skins.empty()) {
        const auto& skin = model.skins[0];
        
        // Safety check: ensure joint count matches
        skin_joint_matrices.resize(skin.joints.size(), glm::mat4(1.0f));

        if (skin.inverseBindMatrices > -1) {
            const auto& accessor = model.accessors[skin.inverseBindMatrices];
            const auto& bufferView = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];
            
            int stride = accessor.ByteStride(bufferView);
            const unsigned char* data_ptr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

            for (size_t i = 0; i < skin.joints.size(); ++i) {
                int jointNodeIdx = skin.joints[i];
                
                // Read IBM (mat4 is 64 bytes)
                const float* m_ptr = reinterpret_cast<const float*>(data_ptr + (i * stride));
                glm::mat4 ibm = glm::make_mat4(m_ptr);
                
                glm::mat4 globalTransform = node_globals[jointNodeIdx];
                
                // Skin Matrix = GlobalTransform * InverseBindMatrix
                // This transforms a vertex from Mesh Space -> Joint Space -> Global Space
                skin_joint_matrices[i] = globalTransform * ibm;
            }
        }
    }
    
    if (default_sampler != nullptr) {
        default_sampler = nullptr;
    }

    std::string base_dir = m_path.substr(0, m_path.find_last_of('/') + 1);
    this->model_path = m_path;

    loadTextures(model, base_dir, engine);
    loadMaterials(model);
    loadGeometry(model);
}

std::map<int, vk::Format> Gameobject::scanTextureFormats(const tinygltf::Model& model) {
    std::map<int, vk::Format> image_formats;

    auto getImageSourceIndex = [&](int texIdx) -> int {
        if (texIdx >= 0 && texIdx < model.textures.size()) {
            return model.textures[texIdx].source;
        }
        return -1;
    };

    for (const auto& mat : model.materials) {
        int sourceIdx = -1;

        // sRGB
        sourceIdx = getImageSourceIndex(mat.pbrMetallicRoughness.baseColorTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Srgb;
        sourceIdx = getImageSourceIndex(mat.emissiveTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Srgb;

        // Linear
        sourceIdx = getImageSourceIndex(mat.normalTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
        sourceIdx = getImageSourceIndex(mat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
        sourceIdx = getImageSourceIndex(mat.occlusionTexture.index);
        if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;

        if(mat.extensions.count("KHR_materials_transmission")){
            const auto& transmission = mat.extensions.at("KHR_materials_transmission");
            if(transmission.Has("transmissionTexture")){
                sourceIdx = getImageSourceIndex(transmission.Get("transmissionTexture").Get("index").Get<int>());
                if(sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
            }
        }

        if(mat.extensions.count("KHR_materials_clearcoat")){
            const auto& clearcoat = mat.extensions.at("KHR_materials_clearcoat");
            if(clearcoat.Has("clearcoatTexture")){
                sourceIdx = getImageSourceIndex(clearcoat.Get("clearcoatTexture").Get("index").Get<int>());
                if(sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
            }
            if(clearcoat.Has("clearcoatRoughnessTexture")){
                sourceIdx = getImageSourceIndex(clearcoat.Get("clearcoatRoughnessTexture").Get("index").Get<int>());
                if(sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Unorm;
            }
        }
    }
    for (const auto& mat : model.materials) {
        // ... existing checks ...

        // --- NEW: Check for Specular Glossiness Extension ---
        if (mat.extensions.count("KHR_materials_pbrSpecularGlossiness")) {
            const auto& ext = mat.extensions.at("KHR_materials_pbrSpecularGlossiness");
            if (ext.Has("specularGlossinessTexture")) {
                int index = ext.Get("specularGlossinessTexture").Get("index").Get<int>();
                int sourceIdx = getImageSourceIndex(index);
                // The spec says this is sRGB
                if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Srgb;
            }
            if (ext.Has("diffuseTexture")) {
                int index = ext.Get("diffuseTexture").Get("index").Get<int>();
                int sourceIdx = getImageSourceIndex(index);
                if (sourceIdx >= 0) image_formats[sourceIdx] = vk::Format::eR8G8B8A8Srgb;
            }
        }
    }
    return image_formats;
}

void Gameobject::loadTextures(const tinygltf::Model& model, const std::string& base_dir, Engine& engine) {
    std::map<int, vk::Format> image_formats = scanTextureFormats(model);

    // Add default white texture
    textures.emplace_back();
    createDefaultTexture(engine, textures[0], glm::vec4(255, 255, 255, 255)); 

    int image_index = 0;
    uint32_t max_mips = 1;

    for (const auto& image : model.images) {
        std::string texture_path = base_dir + image.uri;
        vk::Format format = vk::Format::eR8G8B8A8Srgb;
        if (image_formats.contains(image_index)) {
            format = image_formats[image_index];
        }

        std::cout << "Creating texture: " << image.uri << std::endl;
        textures.push_back(Image::createTextureImage(engine, texture_path.c_str(), format));
        max_mips = std::max<uint32_t>(max_mips, textures.back().mip_levels);
        image_index++;
    }

    default_sampler = Image::createTextureSampler(engine.physical_device, &engine.logical_device, max_mips);
}

int Gameobject::getTextureIndex(int gltfTexIdx, const tinygltf::Model& model) const {
    if (gltfTexIdx >= 0 && gltfTexIdx < model.textures.size()) {
        int sourceIdx = model.textures[gltfTexIdx].source;
        if (sourceIdx >= 0 && sourceIdx < model.images.size()) {
            return sourceIdx + 1; // +1 to offset for the default texture at index 0
        }
    }
    return 0; // Default texture
}

void Gameobject::loadMaterials(const tinygltf::Model& model) {
    int index = 0;
    for (const auto& mat : model.materials) {
        Material newMaterial{};
        const auto& pbr = mat.pbrMetallicRoughness;

        // Transparency check
        newMaterial.is_transparent = (mat.alphaMode == "BLEND");
        if(mat.doubleSided) newMaterial.is_doublesided = true;
        if(mat.alphaMode == "MASK") newMaterial.alpha_cutoff = static_cast<float>(mat.alphaCutoff);

        if (mat.extensions.count("KHR_materials_pbrSpecularGlossiness")) {
            const auto& ext = mat.extensions.at("KHR_materials_pbrSpecularGlossiness");
            
            newMaterial.use_specular_glossiness_workflow = 1.0f;

            // Diffuse Factor (Replaces Base Color)
            if (ext.Has("diffuseFactor")) {
                const auto& f = ext.Get("diffuseFactor");
                newMaterial.base_color_factor = glm::vec4(f.Get(0).Get<double>(), f.Get(1).Get<double>(), f.Get(2).Get<double>(), f.Get(3).Get<double>());
            } else {
                newMaterial.base_color_factor = glm::vec4(1.0f);
            }

            // Specular Factor (We store this in specular_color_factor)
            if (ext.Has("specularFactor")) {
                const auto& f = ext.Get("specularFactor");
                newMaterial.specular_color_factor = glm::vec3(f.Get(0).Get<double>(), f.Get(1).Get<double>(), f.Get(2).Get<double>());
            } else {
                newMaterial.specular_color_factor = glm::vec3(1.0f);
            }

            // Glossiness Factor (We store this in roughness_factor temporarily)
            if (ext.Has("glossinessFactor")) {
                newMaterial.roughness_factor = static_cast<float>(ext.Get("glossinessFactor").Get<double>());
            } else {
                newMaterial.roughness_factor = 1.0f;
            }

            // Textures
            if (ext.Has("diffuseTexture")) {
                int idx = ext.Get("diffuseTexture").Get("index").Get<int>();
                newMaterial.albedo_texture_index = getTextureIndex(idx, model);
            }
            if (ext.Has("specularGlossinessTexture")) {
                int idx = ext.Get("specularGlossinessTexture").Get("index").Get<int>();
                newMaterial.specular_glossiness_texture_index = getTextureIndex(idx, model);
            }

        }
        else{
            // PBR Base
            const auto& bcf = pbr.baseColorFactor;
            newMaterial.base_color_factor = glm::vec4(bcf[0], bcf[1], bcf[2], bcf[3]);
            newMaterial.metallic_factor = static_cast<float>(pbr.metallicFactor);
            newMaterial.roughness_factor = static_cast<float>(pbr.roughnessFactor);

            newMaterial.albedo_texture_index = getTextureIndex(pbr.baseColorTexture.index, model);
            newMaterial.metallic_roughness_texture_index = getTextureIndex(pbr.metallicRoughnessTexture.index, model);
        }
        // Emissive
        const auto& ef = mat.emissiveFactor;
        newMaterial.emissive_factor = glm::vec3(ef[0], ef[1], ef[2]);
        if (mat.extensions.count("KHR_materials_emissive_strength")) {
            const auto& strength_ext = mat.extensions.at("KHR_materials_emissive_strength");
            if (strength_ext.Has("emissiveStrength")) {
                float strength = static_cast<float>(strength_ext.Get("emissiveStrength").Get<double>());
                newMaterial.emissive_factor *= strength;
            }
        }
        // Texture Indices
        newMaterial.normal_texture_index = getTextureIndex(mat.normalTexture.index, model);
        newMaterial.occlusion_texture_index = getTextureIndex(mat.occlusionTexture.index, model);
        newMaterial.emissive_texture_index = getTextureIndex(mat.emissiveTexture.index, model);
        newMaterial.occlusion_strength = static_cast<float>(mat.occlusionTexture.strength); 

        // Specular
        newMaterial.specular_color_factor = glm::vec3(1.0f);
        newMaterial.specular_factor = 0.5f; 
        if (mat.extensions.count("KHR_materials_specular")) {
            const auto& spec_ext = mat.extensions.at("KHR_materials_specular");
            if (spec_ext.Has("specularFactor")) {
                newMaterial.specular_factor = static_cast<float>(spec_ext.Get("specularFactor").Get<double>());
            }
            if (spec_ext.Has("specularColorFactor")) {
                const auto& c = spec_ext.Get("specularColorFactor");
                if (c.IsArray() && c.ArrayLen() >= 3) {
                    newMaterial.specular_color_factor = glm::vec3(c.Get(0).Get<double>(), c.Get(1).Get<double>(), c.Get(2).Get<double>());
                }
            }
        }

        // Transmission
        if(mat.extensions.count("KHR_materials_transmission")){
            const auto& transmission = mat.extensions.at("KHR_materials_transmission");
            if (transmission.Has("transmissionFactor")){
                newMaterial.transmission_factor = static_cast<float>(transmission.Get("transmissionFactor").Get<double>());
            }
            if(transmission.Has("transmissionTexture")){
                newMaterial.transmission_texture_index = getTextureIndex(transmission.Get("transmissionTexture").Get("index").Get<int>(), model);
            }
            if(newMaterial.transmission_factor > 0.0f || transmission.Has("transmissionTexture")){
                newMaterial.is_transparent = true;
            }
        }

        // Clearcoat
        if(mat.extensions.count("KHR_materials_clearcoat")){
            const auto& clearcoat = mat.extensions.at("KHR_materials_clearcoat");
            if(clearcoat.Has("clearcoatFactor")) newMaterial.clearcoat_factor = static_cast<float>(clearcoat.Get("clearcoatFactor").Get<double>());
            if(clearcoat.Has("clearcoatRoughnessFactor")) newMaterial.clearcoat_roughness_factor = static_cast<float>(clearcoat.Get("clearcoatRoughnessFactor").Get<double>());
            if(clearcoat.Has("clearcoatTexture")) newMaterial.clearcoat_texture_index = getTextureIndex(clearcoat.Get("clearcoatTexture").Get("index").Get<int>(), model);
            if(clearcoat.Has("clearcoatRoughnessTexture")) newMaterial.clearcoat_roughness_texture_index = getTextureIndex(clearcoat.Get("clearcoatRoughnessTexture").Get("index").Get<int>(), model);
        }

        materials.push_back(std::move(newMaterial));
    }

    if (materials.empty()) {
        materials.emplace_back();
        materials.back().albedo_texture_index = 0;
    }
}

void Gameobject::loadGeometry(const tinygltf::Model& model) {
    std::unordered_map<Vertex, uint32_t> unique_vertices{};
    if (model.scenes.empty()) throw std::runtime_error("glTF has no scenes!");

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    for (int nodeIndex : model.scenes[sceneIndex].nodes) {
        processNode(nodeIndex, model, glm::mat4(1.0f), unique_vertices);
    }
}

void Gameobject::processNode(int nodeIndex, const tinygltf::Model& model, const glm::mat4& parentTransform, std::unordered_map<Vertex, uint32_t>& unique_vertices) {
    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 nodeTransform = getNodeTransform(node);
    glm::mat4 localTransform = parentTransform * nodeTransform;
    glm::mat4 meshTransform = (node.skin >= 0) ? parentTransform : localTransform;

    if (node.mesh >= 0) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& primitive : mesh.primitives) {
            loadPrimitive(primitive, model, meshTransform, unique_vertices);
        }
    }

    for (int childIndex : node.children) {
        processNode(childIndex, model, localTransform, unique_vertices);
    }
}

glm::mat4 Gameobject::getNodeTransform(const tinygltf::Node& node) const {
    if (node.matrix.size() == 16) return glm::make_mat4(node.matrix.data());

    glm::vec3 translation(0.0f);
    if (node.translation.size() == 3) translation = glm::make_vec3(node.translation.data());

    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size() == 4) rotation = glm::make_quat(node.rotation.data());

    glm::vec3 scale(1.0f);
    if (node.scale.size() == 3) scale = glm::make_vec3(node.scale.data());

    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

void Gameobject::loadPrimitive(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const glm::mat4& transform, std::unordered_map<Vertex, uint32_t>& unique_vertices) {
    int material_index = (primitive.material >= 0) ? primitive.material : 0; 

    // Emissive extraction
    const Material& mat = materials[material_index];
    bool is_emissive = (glm::length(mat.emissive_factor) > 0.001f);
    
    // --- Attribute Accessors (Condensed) ---
    const tinygltf::Accessor& index_accessor = model.accessors[primitive.indices]; 
    size_t total_index_count = index_accessor.count; 
    
    const tinygltf::Accessor& pos_acc = model.accessors[primitive.attributes.at("POSITION")]; 
    const unsigned char* pos_data = &model.buffers[model.bufferViews[pos_acc.bufferView].buffer].data[model.bufferViews[pos_acc.bufferView].byteOffset + pos_acc.byteOffset]; 
    size_t pos_stride = model.bufferViews[pos_acc.bufferView].byteStride ? model.bufferViews[pos_acc.bufferView].byteStride : sizeof(glm::vec3); 

    const unsigned char* normal_data = nullptr; size_t normal_stride = 0;
    if (primitive.attributes.count("NORMAL")) {
        const auto& acc = model.accessors[primitive.attributes.at("NORMAL")];
        normal_data = &model.buffers[model.bufferViews[acc.bufferView].buffer].data[model.bufferViews[acc.bufferView].byteOffset + acc.byteOffset];
        normal_stride = model.bufferViews[acc.bufferView].byteStride ? model.bufferViews[acc.bufferView].byteStride : sizeof(glm::vec3);
    }

    const unsigned char* tangent_data = nullptr; size_t tangent_stride = 0;
    if (primitive.attributes.count("TANGENT")) {
        const auto& acc = model.accessors[primitive.attributes.at("TANGENT")];
        tangent_data = &model.buffers[model.bufferViews[acc.bufferView].buffer].data[model.bufferViews[acc.bufferView].byteOffset + acc.byteOffset];
        tangent_stride = model.bufferViews[acc.bufferView].byteStride ? model.bufferViews[acc.bufferView].byteStride : sizeof(glm::vec4);
    }

    const unsigned char* uv0_data = nullptr; size_t uv0_stride = 0;
    if (primitive.attributes.count("TEXCOORD_0")) {
        const auto& acc = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        uv0_data = &model.buffers[model.bufferViews[acc.bufferView].buffer].data[model.bufferViews[acc.bufferView].byteOffset + acc.byteOffset];
        uv0_stride = model.bufferViews[acc.bufferView].byteStride ? model.bufferViews[acc.bufferView].byteStride : sizeof(glm::vec2);
    }

    const unsigned char* uv1_data = nullptr; size_t uv1_stride = 0;
    if (primitive.attributes.count("TEXCOORD_1")) {
        const auto& acc = model.accessors[primitive.attributes.at("TEXCOORD_1")];
        uv1_data = &model.buffers[model.bufferViews[acc.bufferView].buffer].data[model.bufferViews[acc.bufferView].byteOffset + acc.byteOffset];
        uv1_stride = model.bufferViews[acc.bufferView].byteStride ? model.bufferViews[acc.bufferView].byteStride : sizeof(glm::vec2);
    }

    const auto& idx_view = model.bufferViews[index_accessor.bufferView];
    const unsigned char* index_data = &model.buffers[idx_view.buffer].data[idx_view.byteOffset + index_accessor.byteOffset];

    // --- SKINNING ACCESSORS ---
    const void* joint_data = nullptr; int joint_stride = 0; int joint_component_type = 0;
    const void* weight_data = nullptr; int weight_stride = 0; int weight_component_type = 0;

    bool has_skin = !skin_joint_matrices.empty() && 
                    primitive.attributes.count("JOINTS_0") && 
                    primitive.attributes.count("WEIGHTS_0");

    if (has_skin) {
        const auto& j_acc = model.accessors[primitive.attributes.at("JOINTS_0")];
        const auto& j_bv = model.bufferViews[j_acc.bufferView];
        joint_data = &model.buffers[j_bv.buffer].data[j_bv.byteOffset + j_acc.byteOffset];
        joint_stride = j_acc.ByteStride(j_bv);
        joint_component_type = j_acc.componentType;

        const auto& w_acc = model.accessors[primitive.attributes.at("WEIGHTS_0")];
        const auto& w_bv = model.bufferViews[w_acc.bufferView];
        weight_data = &model.buffers[w_bv.buffer].data[w_bv.byteOffset + w_acc.byteOffset];
        weight_stride = w_acc.ByteStride(w_bv);
        weight_component_type = w_acc.componentType;
    }

    // --- Loading Geometry ---
    std::vector<uint32_t> local_indices;
    local_indices.reserve(total_index_count);

    for (size_t i = 0; i < total_index_count; i++) {
        uint32_t idx = 0;
        if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) 
            idx = reinterpret_cast<const uint16_t*>(index_data)[i];
        else if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) 
            idx = reinterpret_cast<const uint32_t*>(index_data)[i];
        else 
            idx = reinterpret_cast<const uint8_t*>(index_data)[i];

        Vertex vertex{};
        vertex.color = {1.f, 1.f, 1.f};
        vertex.pos = *reinterpret_cast<const glm::vec3*>(pos_data + (idx * pos_stride));
        
        if (normal_data) vertex.normal = *reinterpret_cast<const glm::vec3*>(normal_data + (idx * normal_stride));
        else vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

        if (tangent_data) vertex.tangent = *reinterpret_cast<const glm::vec4*>(tangent_data + (idx * tangent_stride));
        else vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

        if (uv0_data) {
            glm::vec2 uv = *reinterpret_cast<const glm::vec2*>(uv0_data + (idx * uv0_stride));
            vertex.tex_coord = uv;
        }
        if (uv1_data) {
            glm::vec2 uv = *reinterpret_cast<const glm::vec2*>(uv1_data + (idx * uv1_stride));
            vertex.tex_coord_1 = uv;
        } else {
            vertex.tex_coord_1 = vertex.tex_coord;
        }


        // --- APPLY TRANSFORM OR SKINNING ---
        if (has_skin) {
            glm::uvec4 joints;
            glm::vec4 weights;

            // 1. Read Joints (Handle U8 vs U16)
            const void* j_ptr = (const char*)joint_data + (idx * joint_stride);
            if (joint_component_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const uint16_t* jd = static_cast<const uint16_t*>(j_ptr);
                joints = glm::uvec4(jd[0], jd[1], jd[2], jd[3]);
            } else { // UNSIGNED_BYTE
                const uint8_t* jd = static_cast<const uint8_t*>(j_ptr);
                joints = glm::uvec4(jd[0], jd[1], jd[2], jd[3]);
            }

            // 2. Read Weights (Handle Float) - typically float in desktop GLTF
            const void* w_ptr = (const char*)weight_data + (idx * weight_stride);
            if (weight_component_type == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                weights = *static_cast<const glm::vec4*>(w_ptr);
            } else {
                // Fallback for normalized u8/u16 weights (less common in simple pipelines, but good to handle)
                // For now assuming float as per standard GLTF export settings
                weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            }

            // 3. Normalize Weights (Crucial! Exporters sometimes output non-normalized weights)
            float weightSum = weights.x + weights.y + weights.z + weights.w;
            if (weightSum > 0.0f) {
                weights /= weightSum;
            } else {
                weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // Fallback
            }

            // 4. Calculate Skin Matrix
            glm::mat4 skinMat = 
                weights.x * skin_joint_matrices[joints.x] +
                weights.y * skin_joint_matrices[joints.y] +
                weights.z * skin_joint_matrices[joints.z] +
                weights.w * skin_joint_matrices[joints.w];

            // 5. Apply Skinning
            // skinMat is already in global space (because you computed skin_joint_matrices from global node transforms),
            // so apply it directly to the raw vertex (which must be in mesh-local space).
            vertex.pos = glm::vec3(skinMat * glm::vec4(vertex.pos, 1.0f));

            // normals: use inverse-transpose of skinMat
            vertex.normal = glm::normalize(glm::mat3(glm::transpose(glm::inverse(skinMat))) * vertex.normal);

            // tangent: rotation part only
            if (vertex.tangent.w != 0.0f) {
                glm::vec3 t = glm::vec3(vertex.tangent);
                t = glm::normalize(glm::mat3(skinMat) * t);
                vertex.tangent = glm::vec4(t, vertex.tangent.w);
            }
        } 
        else {
            // static mesh case: now apply the node/world transform exactly once
            vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1.0f)); 
            glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));
            vertex.normal = glm::normalize(normal_matrix * vertex.normal);

            if (vertex.tangent.w != 0.0f) {
                glm::vec3 t = glm::vec3(vertex.tangent);
                t = glm::normalize(glm::mat3(transform) * t);
                vertex.tangent = glm::vec4(t, vertex.tangent.w);
            }
        }

        // Deduplication
        if (!unique_vertices.contains(vertex)) {
            unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex); 
        }
        local_indices.push_back(unique_vertices[vertex]);
    }

    // --- Create Primitive ---
    Primitive new_primitive;
    glm::vec3 bbMin( std::numeric_limits<float>::max());
    glm::vec3 bbMax(-std::numeric_limits<float>::max());
    
    new_primitive.material_index = material_index;
    new_primitive.first_index = static_cast<uint32_t>(indices.size());
    new_primitive.index_count = static_cast<uint32_t>(total_index_count);

    for (uint32_t final_index : local_indices) {
        indices.push_back(final_index);
        const auto& pos = vertices[final_index].pos;
        bbMin = glm::min(bbMin, pos);
        bbMax = glm::max(bbMax, pos);
    }
    new_primitive.center = 0.5f * (bbMin + bbMax);

    for (size_t k = 0; k < local_indices.size(); k += 3) {
        uint32_t idx0 = local_indices[k];
        uint32_t idx1 = local_indices[k+1];
        uint32_t idx2 = local_indices[k+2];

        // Add to main index buffer
        indices.push_back(idx0);
        indices.push_back(idx1);
        indices.push_back(idx2);

        // Update Bounding Box
        const auto& p0 = vertices[idx0].pos;
        const auto& p1 = vertices[idx1].pos;
        const auto& p2 = vertices[idx2].pos;
        
        bbMin = glm::min(bbMin, p0); bbMin = glm::min(bbMin, p1); bbMin = glm::min(bbMin, p2);
        bbMax = glm::max(bbMax, p0); bbMax = glm::max(bbMax, p1); bbMax = glm::max(bbMax, p2);

        // --- NEW: NEE Data Collection ---
        if (is_emissive) {
            // Calculate Area (Vertices are already World Space)
            float area = 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
            
            if (area > 1e-6f) { // Ignore degenerate triangles
                EmissiveTriangle tri;
                tri.index0 = idx0;
                tri.index1 = idx1;
                tri.index2 = idx2;
                tri.material_index = material_index;
                tri.area = area;
                
                emissive_triangles.push_back(tri);
            }
        }
    }

    // --- CRITICAL CHANGE: Push EVERYTHING to o_primitives ---
    // The BLAS builder only iterates o_primitives. We want transparent objects
    // to be in the BLAS so the Ray Tracer hits them.
    o_primitives.push_back(new_primitive);
}