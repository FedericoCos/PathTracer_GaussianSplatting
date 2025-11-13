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
    // --- Descriptor sets data ---
    vk::DescriptorSetLayout *descriptor_layout = nullptr;

    // --- Rendering Data ---
    AllocatedBuffer geometry_buffer; 
    vk::DeviceSize index_buffer_offset = 0;

    std::vector<Primitive> o_primitives; 
    std::vector<Primitive> t_primitives;
    std::vector<std::pair<glm::vec3, glm::vec3>> emissive_primitives;
    std::vector<Material> materials;
    std::vector<AllocatedImage> textures;
    vk::raii::Sampler default_sampler = nullptr;
    AccelerationStructure blas;


    // CPU side geometry. NOTE this could be cleaned after uploading to GPU to save RAM
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // --- Pipeline & Shader Information ---
    PipelineInfo* o_pipeline = nullptr;
    PipelineInfo* t_pipeline = nullptr;

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

    // Rotates the object using Euler angles (in degrees)
    virtual void changeRotation(const glm::vec3 &new_rot_degrees) {
        // Convert degrees to radians for GLM, then create the quaternion
        rotation = glm::quat(glm::radians(new_rot_degrees));
        updateModelMatrix();
    }

    virtual bool inputUpdate(InputState &input, float &dtime);

    virtual void loadModel(std::string m_path, Engine &engine);
    virtual void createMaterialDescriptorSets(Engine& engine);

    void createDefaultTexture(Engine& engine, AllocatedImage& texture, glm::vec4 color);

    std::vector<Pointlight> createEmissiveLights(float intensity);
    

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
    // Forward declarations for tinygltf types

    // Main loading stages
    void loadTextures(const tinygltf::Model& model, const std::string& base_dir, Engine& engine);
    void loadMaterials(const tinygltf::Model& model);
    void loadGeometry(const tinygltf::Model& model);

    // Geometry helpers
    void processNode(int nodeIndex, const tinygltf::Model& model, const glm::mat4& parentTransform, std::unordered_map<Vertex, uint32_t>& unique_vertices);
    void loadPrimitive(const tinygltf::Primitive& primitive, const tinygltf::Model& model, const glm::mat4& transform, std::unordered_map<Vertex, uint32_t>& unique_vertices);
    
    // Utility helpers
    std::map<int, vk::Format> scanTextureFormats(const tinygltf::Model& model);
    glm::mat4 getNodeTransform(const tinygltf::Node& node) const;
    int getTextureIndex(int gltfTexIdx, const tinygltf::Model& model) const;
};
