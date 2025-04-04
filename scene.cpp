// Manages scene resources and objects
#include "scene.h"

VulkanScene::VulkanScene(VulkanEngine& engine, std::vector<std::string> asset_paths){
    for(std::string& path : asset_paths){
        auto loaded = loadGltf(engine, path);
        assert(loaded.has_value());

        size_t lastSlash = path.find_last_of('/');
        std::string filenameWithExt = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        size_t lastDot = filenameWithExt.find_last_of('.');
        std::string filename = (lastDot == std::string::npos) ? filenameWithExt : filenameWithExt.substr(0, lastDot);

        loadedAsset[filename] = *loaded;
    }

    for (auto& asset : loadedAsset) {
        for (auto& node : asset.second.get()->topNodes) {
            topNodes[node.get() -> name] = node;
        }
    }
}

void VulkanScene::update(float& deltatime){
    if(_update){
        glm::vec3 scaledRotation = _rotationVector * deltatime;

        glm::mat4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(scaledRotation[0]), glm::vec3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(scaledRotation[1]), glm::vec3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(scaledRotation[2]), glm::vec3(0.0f, 0.0f, 1.0f));

        for (auto& asset : loadedAsset) {
            asset.second.get()->updateNodesRotation(rotationMatrix);
        }
    }
}

void VulkanScene::draw(DrawContext& drawContext){
    for(auto& asset : loadedAsset){
        asset.second.get() ->Draw(glm::mat4{1.f}, drawContext);
    }
}

void VulkanScene::set_imgui(){
    ImGui::Begin("Scene Setting");
    ImGui::Checkbox("Update structure", &_update);
    ImGui::InputFloat3("rot", (float*)&_rotationVector);

    ImGui::BeginChild("Objects", ImVec2(0, 0), true);

    for (auto& node : topNodes) { 
        recursive_node_imgui(*node.second.get());
    }

    ImGui::EndChild();
    ImGui::End();
}

void VulkanScene::recursive_node_imgui(Node& n){
    if (ImGui::TreeNode(n.name.c_str())){
        for (auto& child : n.children) { 
            recursive_node_imgui(*child.get());
        }
        ImGui::TreePop();
    }
}
