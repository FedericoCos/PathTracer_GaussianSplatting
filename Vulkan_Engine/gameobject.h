#pragma once

#include "../Helpers/GeneralHeaders.h"

class Engine; // Forward declaration

class Gameobject{
public:
    // --- Rendering Data ---
    AllocatedBuffer buffer;
    AllocatedImage texture;
    vk::raii::Sampler texture_sampler = nullptr;
    vk::DeviceSize buffer_index_offset; 
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::vector<vk::raii::DescriptorSet> descriptor_sets;

    // --- Pipeline & Shader Information ---
    std::string vertex_shader;
    std::string fragment_shader;
    vk::raii::Pipeline *graphics_pipeline = nullptr;
    vk::raii::PipelineLayout *pipeline_layout = nullptr;

    // --- Model and texture path ---
    std::string model_path;
    std::string texture_path;

    // --- Transform ---
    glm::mat4 model_matrix = glm::mat4(1.f);
    bool isVisible = true;

    Gameobject() {
        vertex_shader = "shaders/basic/vertex.spv";
        fragment_shader = "shaders/basic/fragment.spv";
    }

    Gameobject(std::string path_vertex, std::string path_fragment){
        vertex_shader = path_vertex;
        fragment_shader = path_fragment;
    }

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

    virtual bool inputUpdate(InputState &input, float &dtime) = 0;

    virtual void loadModel(std::string m_path, std::string t_path, Engine &engine);
    virtual void createDescriptorSets(Engine& engine);
    

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
