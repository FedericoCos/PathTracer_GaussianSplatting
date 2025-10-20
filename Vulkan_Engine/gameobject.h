#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward declaration

class Gameobject{
public:
    // --- Rendering Data ---
    AllocatedBuffer geometry_buffer; //  A single buffer for all VBO/IBO data
    vk::DeviceSize index_buffer_offset = 0; // Offset into geometry_buffer for indices

    std::vector<Primitive> primitives; // all sub-meshes to draw
    std::vector<Material> materials;
    std::vector<AllocatedImage> textures;
    vk::raii::Sampler default_sampler = nullptr;


    // CPU side geometry. NOTE this could be cleaned after uploading to GPU to save RAM
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // --- Pipeline & Shader Information ---
    PipelineInfo* pipeline = nullptr;

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
    // virtual void createDescriptorSets(Engine& engine);
    virtual void createMaterialDescriptorSets(Engine& engine);
    

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
};
