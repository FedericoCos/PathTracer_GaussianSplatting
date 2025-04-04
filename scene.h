// Manages scene resources and objects
#pragma once

#include "vk_engine.h"

class VulkanScene{
private:
    

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedAsset;
    std::unordered_map<std::string, std::shared_ptr<Node>> topNodes;

    bool _update{false};

    glm::mat4 rotationMatrix = glm::mat4(1.f);
    glm::vec3 _rotationVector = glm::vec3(0.f);


public:

    VulkanScene(VulkanEngine& engine, std::vector<std::string> asset_paths);

    void update(float& deltatime);
    void draw(DrawContext& drawContext);

    void set_imgui();
    void recursive_node_imgui(Node& n);



};
