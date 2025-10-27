#include "gameobject.h"
#include "engine.h"
#include "image.h"

// --- HELPER FUNCTION (add this to the top of gameobject.cpp) ---
void createDefaultTexture(Engine& engine, AllocatedImage& texture, glm::vec4 color) {
    unsigned char colored_pixel[] = {color.x, color.y, color.z, color.w};
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

// ... (loadModel, scanTextureFormats, loadTextures, getTextureIndex, loadMaterials, loadGeometry, processNode, getNodeTransform all remain unchanged) ...
void Gameobject::loadModel(std::string m_path, Engine &engine)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // --- 1. File Loading ---
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

    // --- 2. Clear previous data ---
    vertices.clear();
    indices.clear();
    textures.clear();
    materials.clear();
    o_primitives.clear();
    t_primitives.clear(); // <-- Make sure to clear transparent primitives too
    if (default_sampler != nullptr) {
        default_sampler = nullptr; // vk::raii::Sampler will handle destruction
    }

    std::string base_dir = m_path.substr(0, m_path.find_last_of('/') + 1);
    this->model_path = m_path;

    // --- 3. Load Model Components in Stages ---
    loadTextures(model, base_dir, engine);
    loadMaterials(model);
    loadGeometry(model);
}

std::map<int, vk::Format> Gameobject::scanTextureFormats(const tinygltf::Model& model) {
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

        // Check for clearcoat extensions
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
    return image_formats;
}

void Gameobject::loadTextures(const tinygltf::Model& model, const std::string& base_dir, Engine& engine) {
    // Pre-scan materials to determine correct texture formats (sRGB vs linear)
    std::map<int, vk::Format> image_formats = scanTextureFormats(model);

    // Add default textures
    textures.emplace_back();
    createDefaultTexture(engine, textures[0], glm::vec4(1, 1, 1, 1)); 

    int image_index = 0;
    uint32_t max_mips = 1;

    // Load all images from the glTF file
    for (const auto& image : model.images) {
        std::string texture_path = base_dir + image.uri;
        
        // Default to sRGB, but use scanned formimage_indexat if available
        vk::Format format = vk::Format::eR8G8B8A8Srgb;
        if (image_formats.contains(image_index)) {
            format = image_formats[image_index];
        }

        textures.push_back(Image::createTextureImage(engine, texture_path.c_str(), format));

        // Track the highest mip level for the sampler
        max_mips = std::max<uint32_t>(max_mips, textures.back().mip_levels);
    }

    // Create a single sampler for all textures
    default_sampler = Image::createTextureSampler(engine.physical_device, &engine.logical_device, max_mips);
}

int Gameobject::getTextureIndex(int gltfTexIdx, const tinygltf::Model& model) const {
    if (gltfTexIdx >= 0 && gltfTexIdx < model.textures.size()) {
        int sourceIdx = model.textures[gltfTexIdx].source;
        if (sourceIdx >= 0 && sourceIdx < model.images.size()) {
            return sourceIdx + 1; // +1 for default texture
        }
    }
    // Return 0 (default white texture) if invalid
    return 0; 
}

void Gameobject::loadMaterials(const tinygltf::Model& model) {
    bool first = true;
    std::cout << "MATERIALS SIZE: " << model.materials.size() << std::endl;
    const float ALPHA_OPACITY_THRESHOLD = 0.95f; 

    for (const auto& mat : model.materials) {
        Material newMaterial{};
        const auto& pbr = mat.pbrMetallicRoughness;

        bool is_blend = (mat.alphaMode == "BLEND");
        if (!is_blend) {
            newMaterial.is_transparent = false; // OPAQUE mode
        } else {
            // It's BLEND mode. Check if we can optimize it to be opaque.
            // We can only do this if there is NO alpha texture.
            bool has_alpha_texture = (pbr.baseColorTexture.index >= 0);
            float base_alpha = static_cast<float>(pbr.baseColorFactor[3]);

            if (has_alpha_texture) {
                // We can't read the texture's alpha values on the CPU, so to be safe,
                // we must treat it as transparent if the material uses an alpha channel.
                newMaterial.is_transparent = true; 
            } else {
                // No alpha texture. The *only* source of alpha is the base_color_factor.
                // We can safely check this factor against our threshold.
                if (base_alpha < ALPHA_OPACITY_THRESHOLD) {
                    newMaterial.is_transparent = true; // Genuinely transparent
                } else {
                    newMaterial.is_transparent = false; // e.g., alpha is 0.999. Treat as opaque.
                }
            }
        }

        // --- PBR Base ---
        const auto& bcf = pbr.baseColorFactor;
        newMaterial.base_color_factor = glm::vec4(bcf[0], bcf[1], bcf[2], bcf[3]);
        newMaterial.metallic_factor = static_cast<float>(pbr.metallicFactor);
        newMaterial.roughness_factor = static_cast<float>(pbr.roughnessFactor);

        // --- Emissive ---
        const auto& ef = mat.emissiveFactor;
        newMaterial.emissive_factor = glm::vec3(ef[0], ef[1], ef[2]);
        if (mat.extensions.count("KHR_materials_emissive_strength")) {
            const auto& strength_ext = mat.extensions.at("KHR_materials_emissive_strength");
            if (strength_ext.Has("emissiveStrength")) {
                float strength = static_cast<float>(strength_ext.Get("emissiveStrength").Get<double>());
                newMaterial.emissive_factor *= strength;
            }
        }

        // --- Texture Indices ---
        newMaterial.albedo_texture_index = getTextureIndex(pbr.baseColorTexture.index, model);
        newMaterial.normal_texture_index = getTextureIndex(mat.normalTexture.index, model);
        newMaterial.metallic_roughness_texture_index = getTextureIndex(pbr.metallicRoughnessTexture.index, model);
        newMaterial.occlusion_texture_index = getTextureIndex(mat.occlusionTexture.index, model);
        newMaterial.emissive_texture_index = getTextureIndex(mat.emissiveTexture.index, model);

        // --- Occlusion ---
        newMaterial.occlusion_strength = static_cast<float>(mat.occlusionTexture.strength); 

        
        // --- Specular Extension ---
        newMaterial.specular_color_factor = glm::vec3(1.0f);
        newMaterial.specular_factor = 0.5f; // Default for IOR 1.5

        if (mat.extensions.count("KHR_materials_specular")) {
            const auto& spec_ext = mat.extensions.at("KHR_materials_specular");
            
            if (spec_ext.Has("specularFactor")) {
                newMaterial.specular_factor = static_cast<float>(spec_ext.Get("specularFactor").Get<double>());
            }
            
            if (spec_ext.Has("specularColorFactor")) {
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

        // --- Clearcoat Extension ---
        if (mat.extensions.count("KHR_materials_clearcoat")) {
            const auto& cc_ext = mat.extensions.at("KHR_materials_clearcoat");

            if (cc_ext.Has("clearcoatFactor")) {
                newMaterial.clearcoat_factor = static_cast<float>(cc_ext.Get("clearcoatFactor").Get<double>());
            }
            if (cc_ext.Has("clearcoatRoughnessFactor")) {
                newMaterial.clearcoat_roughness_factor = static_cast<float>(cc_ext.Get("clearcoatRoughnessFactor").Get<double>());
            }
            if (cc_ext.Has("clearcoatTexture")) {
                newMaterial.clearcoat_texture_index = getTextureIndex(cc_ext.Get("clearcoatTexture").Get("index").Get<int>(), model);
            }
            if (cc_ext.Has("clearcoatRoughnessTexture")) {
                newMaterial.clearcoat_roughness_texture_index = getTextureIndex(cc_ext.Get("clearcoatRoughnessTexture").Get("index").Get<int>(), model);
            }
        }

        materials.push_back(std::move(newMaterial));
    }

    // Ensure at least one default material exists
    if (materials.empty()) {
        materials.emplace_back();
        materials.back().albedo_texture_index = 0; // Use default white texture
    }
}

void Gameobject::loadGeometry(const tinygltf::Model& model) {
    // Map to deduplicate vertices
    std::unordered_map<Vertex, uint32_t> unique_vertices{};

    // --- Start Node Traversal ---
    if (model.scenes.empty()) {
        throw std::runtime_error("glTF has no scenes!");
    }

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    for (int nodeIndex : model.scenes[sceneIndex].nodes) {
        // Start recursion with identity matrix
        processNode(nodeIndex, model, glm::mat4(1.0f), unique_vertices);
    }
}

void Gameobject::processNode(int nodeIndex, const tinygltf::Model& model, const glm::mat4& parentTransform, std::unordered_map<Vertex, uint32_t>& unique_vertices) {
    
    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 localTransform = parentTransform * getNodeTransform(node);

    // Load all primitives for the mesh associated with this node
    if (node.mesh >= 0) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& primitive : mesh.primitives) {
            loadPrimitive(primitive, model, localTransform, unique_vertices);
        }
    }

    // Recursively process all children
    for (int childIndex : node.children) {
        processNode(childIndex, model, localTransform, unique_vertices);
    }
}

glm::mat4 Gameobject::getNodeTransform(const tinygltf::Node& node) const {
    // Check for a full matrix first
    if (node.matrix.size() == 16) {
        return glm::make_mat4(node.matrix.data());
    }

    // Otherwise, compose from TRS (Translation, Rotation, Scale)
    glm::vec3 translation(0.0f);
    if (node.translation.size() == 3) {
        translation = glm::make_vec3(node.translation.data());
    }

    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size() == 4) {
        rotation = glm::make_quat(node.rotation.data());
    }

    glm::vec3 scale(1.0f);
    if (node.scale.size() == 3) {
        scale = glm::make_vec3(node.scale.data());
    }

    // T * R * S
    return glm::translate(glm::mat4(1.0f), translation)
         * glm::mat4_cast(rotation)
         * glm::scale(glm::mat4(1.0f), scale);
}


/**
 * @brief Helper struct for the transparent batching process.
 * Stores the indices and calculated center for one connected component.
 */
struct TempComponent {
    std::vector<uint32_t> component_indices; // Indices using global vertex indices
    glm::vec3 center;
};


// --- NEW ---
/**
 * @brief Custom comparator for glm::ivec3 keys in std::map.
 * Performs a lexicographical comparison (x, then y, then z).
 */
struct CompareVec3 {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};
// --- END NEW ---


/**
 * @brief Loads a single mesh primitive.
 * - OPAQUE primitives are loaded as a single struct.
 * - TRANSPARENT primitives are:
 * 1. Split into connected components.
 * 2. Batched into spatial grid cells to reduce draw calls.
 */
void Gameobject::loadPrimitive(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const glm::mat4& transform, std::unordered_map<Vertex, uint32_t>& unique_vertices) {
    
    // --- 0a. CHOOSE TRANSPARENT BATCHING STRATEGY ---
    // true:  Group nearby components into grid cells. (Fewer draw calls, less accurate sorting)
    // false: Create one primitive per connected component. (More draw calls, perfect component sorting)
    const bool BATCH_NEARBY_COMPONENTS = true; 
    // ---

    // --- 0b. DEFINE YOUR BATCHING RESOLUTION ---
    /**
     * @brief Controls the size of the 3D grid for batching.
     * A smaller value (e.g., 0.25f) = more batches, better sorting.
     * A larger value (e.g., 2.0f) = fewer batches, worse sorting.
     * This is in world units (assuming 1.0 = 1 meter).
     */
    const float TRANSPARENT_BATCH_GRID_SIZE = 1.0f; 
    // ---

    // --- 1. Get Material and Index Data ---
    int material_index = (primitive.material >= 0) ? primitive.material : 0; 
    bool is_transparent = materials[material_index].is_transparent;
    

    const tinygltf::Accessor& index_accessor = model.accessors[primitive.indices]; 
    size_t total_index_count = index_accessor.count; 

    // --- 2. Get Pointers to Attribute Data ---
    // (This section is unchanged - it just gets data pointers)
    const tinygltf::Accessor& pos_accessor = model.accessors[primitive.attributes.at("POSITION")]; 
    const tinygltf::BufferView& pos_buffer_view = model.bufferViews[pos_accessor.bufferView]; 
    const unsigned char* pos_data = &model.buffers[pos_buffer_view.buffer].data[pos_buffer_view.byteOffset + pos_accessor.byteOffset]; 
    size_t pos_stride = pos_buffer_view.byteStride ? pos_buffer_view.byteStride : sizeof(glm::vec3); 
    const unsigned char* normal_data = nullptr;
    size_t normal_stride = 0;
    bool has_normals = primitive.attributes.count("NORMAL"); 
    if (has_normals) {
        const tinygltf::Accessor& normal_accessor = model.accessors[primitive.attributes.at("NORMAL")]; 
        const tinygltf::BufferView& normal_buffer_view = model.bufferViews[normal_accessor.bufferView]; 
        normal_data = &model.buffers[normal_buffer_view.buffer].data[normal_buffer_view.byteOffset + normal_accessor.byteOffset]; 
        normal_stride = normal_buffer_view.byteStride ? normal_buffer_view.byteStride : sizeof(glm::vec3); 
    }
    const unsigned char* tangent_data = nullptr;
    size_t tangent_stride = 0;
    bool has_tangents = primitive.attributes.count("TANGENT"); 
    if (has_tangents) {
        const tinygltf::Accessor& tangent_accessor = model.accessors[primitive.attributes.at("TANGENT")]; 
        const tinygltf::BufferView& tangent_buffer_view = model.bufferViews[tangent_accessor.bufferView]; 
        tangent_data = &model.buffers[tangent_buffer_view.buffer].data[tangent_buffer_view.byteOffset + tangent_accessor.byteOffset]; 
        tangent_stride = tangent_buffer_view.byteStride ? tangent_buffer_view.byteStride : sizeof(glm::vec4); 
    }
    const unsigned char* tex_coord_data = nullptr;
    size_t tex_coord_stride = 0;
    bool has_tex_coords = primitive.attributes.count("TEXCOORD_0"); 
    if (has_tex_coords) {
        const tinygltf::Accessor& tex_coord_accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")]; 
        const tinygltf::BufferView& tex_coord_buffer_view = model.bufferViews[tex_coord_accessor.bufferView]; 
        tex_coord_data = &model.buffers[tex_coord_buffer_view.buffer].data[tex_coord_buffer_view.byteOffset + tex_coord_accessor.byteOffset]; 
        tex_coord_stride = tex_coord_buffer_view.byteStride ? tex_coord_buffer_view.byteStride : sizeof(glm::vec2); 
    }
    const unsigned char* tex_coord_data_1 = nullptr;
    size_t tex_coord_stride_1 = 0;
    bool has_tex_coords_1 = primitive.attributes.count("TEXCOORD_1"); 
    if (has_tex_coords_1) {
        const tinygltf::Accessor& tex_coord_accessor = model.accessors[primitive.attributes.at("TEXCOORD_1")]; 
        const tinygltf::BufferView& tex_coord_buffer_view = model.bufferViews[tex_coord_accessor.bufferView]; 
        tex_coord_data_1 = &model.buffers[tex_coord_buffer_view.buffer].data[tex_coord_buffer_view.byteOffset + tex_coord_accessor.byteOffset]; 
        tex_coord_stride_1 = tex_coord_buffer_view.byteStride ? tex_coord_buffer_view.byteStride : sizeof(glm::vec2); 
    }
    const tinygltf::BufferView& index_buffer_view = model.bufferViews[index_accessor.bufferView]; 
    const unsigned char* index_data = &model.buffers[index_buffer_view.buffer].data[index_buffer_view.byteOffset + index_accessor.byteOffset]; 

    // --- 3. Process Vertices based on transparency ---

    // Temporary storage for *all* final, deduplicated indices for this glTF primitive
    std::vector<uint32_t> local_final_indices;
    local_final_indices.reserve(total_index_count);

    // --- 3a. First Pass: Load all vertices and local indices ---
    for (size_t i = 0; i < total_index_count; i++) {
        uint32_t orig_idx = 0;
        switch (index_accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                orig_idx = reinterpret_cast<const uint8_t*>(index_data)[i]; break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                orig_idx = reinterpret_cast<const uint16_t*>(index_data)[i]; break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                orig_idx = reinterpret_cast<const uint32_t*>(index_data)[i]; break;
        }

        Vertex vertex{};
        vertex.color = {1.f, 1.f, 1.f};
        vertex.pos = *reinterpret_cast<const glm::vec3*>(pos_data + (orig_idx * pos_stride));
        vertex.normal = has_normals ? *reinterpret_cast<const glm::vec3*>(normal_data + (orig_idx * normal_stride)) : glm::vec3(0.0f, 0.0f, 1.0f);
        vertex.tangent = has_tangents ? *reinterpret_cast<const glm::vec4*>(tangent_data + (orig_idx * tangent_stride)) : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (has_tex_coords) {
            glm::vec2 raw_tex = *reinterpret_cast<const glm::vec2*>(tex_coord_data + (orig_idx * tex_coord_stride));
            vertex.tex_coord = {raw_tex.x, raw_tex.y};
        }
        if (has_tex_coords_1) {
            glm::vec2 raw_tex = *reinterpret_cast<const glm::vec2*>(tex_coord_data_1 + (orig_idx * tex_coord_stride_1));
            vertex.tex_coord_1 = {raw_tex.x, raw_tex.y};
        } else if (has_tex_coords) {
            vertex.tex_coord_1 = vertex.tex_coord;
        }

        vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1.0f)); 
        vertex.normal = glm::normalize(glm::mat3(transform) * vertex.normal); 

        if (!unique_vertices.contains(vertex)) {
            unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex); 
        }
        local_final_indices.push_back(unique_vertices[vertex]);
    }

    // --- 3b. LOGIC BRANCH ---
    if (is_transparent) {
        // --- TRANSPARENT path: Build graph, find components, and batch them ---

        // ----- 3b.1. Find all connected components -----
        struct Triangle { uint32_t v[3]; };
        std::vector<Triangle> local_triangles;
        local_triangles.reserve(total_index_count / 3);

        auto make_sorted_pair = [](uint32_t a, uint32_t b) {
            return std::make_pair(std::min(a, b), std::max(a, b));
        };
        std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> edge_to_triangles;
        
        for (size_t i = 0; i < local_final_indices.size(); i += 3) {
            uint32_t v0 = local_final_indices[i];
            uint32_t v1 = local_final_indices[i+1];
            uint32_t v2 = local_final_indices[i+2];
            uint32_t tri_idx = static_cast<uint32_t>(local_triangles.size());
            local_triangles.push_back({v0, v1, v2});
            edge_to_triangles[make_sorted_pair(v0, v1)].push_back(tri_idx);
            edge_to_triangles[make_sorted_pair(v1, v2)].push_back(tri_idx);
            edge_to_triangles[make_sorted_pair(v2, v0)].push_back(tri_idx);
        }

        size_t num_triangles = local_triangles.size();
        std::vector<std::vector<uint32_t>> adj(num_triangles);
        for (auto const& [edge, tris] : edge_to_triangles) {
            if (tris.size() > 1) {
                for (size_t i = 0; i < tris.size(); ++i) {
                    for (size_t j = i + 1; j < tris.size(); ++j) {
                        adj[tris[i]].push_back(tris[j]);
                        adj[tris[j]].push_back(tris[i]);
                    }
                }
            }
        }

        std::vector<TempComponent> all_components; // Store all found components here
        std::vector<bool> visited(num_triangles, false);
        
        for (uint32_t i = 0; i < num_triangles; ++i) {
            if (!visited[i]) {
                // Start of a new component
                TempComponent newComponent;
                glm::vec3 center_sum(0.0f);
                float vertex_count = 0.0f;

                std::stack<uint32_t> stack;
                stack.push(i);
                visited[i] = true;

                while (!stack.empty()) {
                    uint32_t current_tri_idx = stack.top();
                    stack.pop();
                    const auto& tri = local_triangles[current_tri_idx];

                    // Add this triangle's indices to the component's list
                    newComponent.component_indices.push_back(tri.v[0]);
                    newComponent.component_indices.push_back(tri.v[1]);
                    newComponent.component_indices.push_back(tri.v[2]);

                    center_sum += vertices[tri.v[0]].pos;
                    center_sum += vertices[tri.v[1]].pos;
                    center_sum += vertices[tri.v[2]].pos;
                    vertex_count += 3.0f;

                    for (uint32_t neighbor_idx : adj[current_tri_idx]) {
                        if (!visited[neighbor_idx]) {
                            visited[neighbor_idx] = true;
                            stack.push(neighbor_idx);
                        }
                    }
                }
                newComponent.center = center_sum / vertex_count;
                all_components.push_back(std::move(newComponent));
            }
        }

        if (BATCH_NEARBY_COMPONENTS) {
            // ----- 3b.2. Batch components into grid cells -----
            
            // Key: Voxel/grid cell coordinate
            // Value: A list of all components that fall into this cell
            std::map<glm::ivec3, std::vector<TempComponent*>, CompareVec3> batches;

            for (auto& component : all_components) {
                glm::ivec3 cell_coord = glm::floor(component.center / TRANSPARENT_BATCH_GRID_SIZE);
                batches[cell_coord].push_back(&component); // This line now works
            }

            // ----- 3b.3. Create final primitives from batches -----
            
            for (auto& [cell, component_list] : batches) {
                // This component_list is our new, batched primitive.
                Primitive newPrimitive;
                newPrimitive.material_index = material_index;
                newPrimitive.first_index = static_cast<uint32_t>(indices.size()); // New batch starts here
                newPrimitive.index_count = 0;
                
                glm::vec3 weighted_center_sum(0.0f);
                float total_triangles = 0.0f;

                // Add all indices from all components in this batch
                for (TempComponent* comp : component_list) {
                    // Copy all indices from the component into the GLOBAL indices buffer
                    indices.insert(indices.end(), comp->component_indices.begin(), comp->component_indices.end());
                    
                    float tri_count = comp->component_indices.size() / 3.0f;
                    newPrimitive.index_count += static_cast<uint32_t>(comp->component_indices.size());
                    
                    // Weight the center by the number of triangles
                    weighted_center_sum += comp->center * tri_count;
                    total_triangles += tri_count;
                }

                // Calculate the final weighted-average center for sorting
                if (total_triangles > 0) {
                    newPrimitive.center = weighted_center_sum / total_triangles;
                } else {
                    newPrimitive.center = glm::vec3(0.0f); // Should not happen
                }

                t_primitives.push_back(newPrimitive);
            }
        } else {
            // ----- 3b.2 (Alternate): Create one primitive per component -----
            for (auto& comp : all_components) {
                Primitive newPrimitive;
                newPrimitive.material_index = material_index;
                newPrimitive.first_index = static_cast<uint32_t>(indices.size()); // New primitive starts here
                newPrimitive.index_count = static_cast<uint32_t>(comp.component_indices.size());
                newPrimitive.center = comp.center;

                // Copy this component's indices into the GLOBAL index buffer
                indices.insert(indices.end(), comp.component_indices.begin(), comp.component_indices.end());

                t_primitives.push_back(newPrimitive);
            }
        }

    } else {
        // --- OPAQUE path: Create ONE primitive (Unchanged) ---
        Primitive opaquePrimitive;
        glm::vec3 bbMin( std::numeric_limits<float>::max());
        glm::vec3 bbMax(-std::numeric_limits<float>::max());
        
        opaquePrimitive.material_index = material_index;
        opaquePrimitive.first_index = static_cast<uint32_t>(indices.size());
        opaquePrimitive.index_count = static_cast<uint32_t>(total_index_count);

        for (uint32_t final_index : local_final_indices) {
            indices.push_back(final_index);
            const auto& pos = vertices[final_index].pos;
            bbMin = glm::min(bbMin, pos);
            bbMax = glm::max(bbMax, pos);
        }

        opaquePrimitive.center = 0.5f * (bbMin + bbMax);
        o_primitives.push_back(opaquePrimitive);
    }
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
        std::vector<vk::DescriptorSetLayout> layouts;
        if(material.is_transparent){
            layouts = std::vector<vk::DescriptorSetLayout>(MAX_FRAMES_IN_FLIGHT, *t_pipeline->descriptor_set_layout);
        }
        else{
            layouts = std::vector<vk::DescriptorSetLayout>(MAX_FRAMES_IN_FLIGHT, *o_pipeline->descriptor_set_layout);
        }
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
            clearcoat_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

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
            descriptor_writes[7].pImageInfo = material.clearcoat_roughness_texture_index != 0 ? &clearcoat_roughness_info : &clearcoat_info;

            engine.logical_device.updateDescriptorSets(descriptor_writes, {});
        }
    }
}

