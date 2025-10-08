#pragma once

#include "../Helpers/GeneralHeaders.h"

class Gameobject{
public:
    // --- Rendering Data ---
    AllocatedBuffer obj_buffer;
    vk::DeviceSize obj_index_offset; 
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

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

    virtual void loadModel(std::string path){
        tinyobj::attrib_t attrib; // COntains all the positions, normals and texture coord
        std::vector<tinyobj::shape_t> shapes; // Contains all the separate objects and their faces
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())){
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
