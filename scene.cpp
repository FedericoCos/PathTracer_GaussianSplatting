// Manages scene resources and objects
#include "scene.h"


void VulkanScene::update(float& deltatime){
    if(_update){
        for(auto& asset : loadedAsset){
            asset.second.get() ->updateNodesRotation(glm::radians(_angle) * deltatime);
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
    ImGui::InputFloat("rot", &_angle);
    ImGui::End();
}
