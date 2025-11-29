#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward declaration

namespace tinygltf { // Forward declaration
    class Model;
    class Node;
    class Primitive;
};

class Gameobject{
public:
    // --- Rendering Data ---
    AllocatedBuffer geometry_buffer; 
    vk::DeviceSize index_buffer_offset = 0;

    // We store all primitives here so the BLAS builder sees everything
    std::vector<Primitive> o_primitives; 
    std::vector<EmissiveTriangle> emissive_triangles;
    
    std::vector<Material> materials;
    std::vector<AllocatedImage> textures;
    vk::raii::Sampler default_sampler = nullptr;
    AccelerationStructure blas;
    
    // Offsets into the Global Bindless Buffers
    uint32_t material_buffer_offset = 0;
    uint32_t mesh_info_offset = 0;

    // CPU side geometry (Uploads to Global Buffers)
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // --- Model and texture path ---
    std::string model_path;

    // --- Transform ---
    glm::mat4 model_matrix = glm::mat4(1.f);
    bool isVisible = true;

    // --- Virtual Transform Methods ---
    virtual void changePosition(const glm::vec3 &new_pos) {
        position = new_pos;
        updateModelMatrix();
    }
    
    virtual void changeScale(const glm::vec3 &new_scale) {
        scale = new_scale;
        updateModelMatrix();
    }

    virtual void changeRotation(const glm::vec3 &new_rot_degrees) {
        rotation = glm::quat(glm::radians(new_rot_degrees));
        updateModelMatrix();
    }

    virtual bool inputUpdate(InputState &input, float &dtime);

    virtual void loadModel(std::string m_path, Engine &engine);
    
    // Creates a 1x1 pixel texture (used for white/default textures)
    void createDefaultTexture(Engine& engine, AllocatedImage& texture, glm::vec4 color);

    

protected:
    // --- Transform Components ---
    glm::vec3 position = glm::vec3(0.f);
    glm::vec3 scale = glm::vec3(1.f);
    glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);

    void updateModelMatrix(){
        glm::mat4 trans_matrix = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rot_matrix = glm::mat4_cast(rotation);
        glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), scale);

        model_matrix = trans_matrix * rot_matrix * scale_matrix;
    }

    // --- glTF Loading Helpers ---
    void loadTextures(const tinygltf::Model& model, const std::string& base_dir, Engine& engine);
    void loadMaterials(const tinygltf::Model& model);
    void loadGeometry(const tinygltf::Model& model);

    void processNode(int nodeIndex, const tinygltf::Model& model, const glm::mat4& parentTransform, std::unordered_map<Vertex, uint32_t>& unique_vertices);
    void loadPrimitive(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const glm::mat4& transform, std::unordered_map<Vertex, uint32_t>& unique_vertices);
    
    std::map<int, vk::Format> scanTextureFormats(const tinygltf::Model& model);
    glm::mat4 getNodeTransform(const tinygltf::Node& node) const;
    int getTextureIndex(int gltfTexIdx, const tinygltf::Model& model) const;
};