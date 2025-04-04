// Manages scene resources and objects
#pragma once

#include "vk_engine.h"

class VulkanScene{
private:
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedAsset;
    std::unordered_map<std::string, std::shared_ptr<MeshNode>> objects;

    bool _update{false};
    float _angle = 10.f;


public:

    VulkanScene(VulkanEngine& engine, std::vector<std::string> asset_paths){
        for(std::string& path : asset_paths){
            auto loaded = loadGltf(engine, path);
            assert(loaded.has_value());

            size_t lastSlash = path.find_last_of('/');
            std::string filenameWithExt = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
            size_t lastDot = filenameWithExt.find_last_of('.');
            std::string filename = (lastDot == std::string::npos) ? filenameWithExt : filenameWithExt.substr(0, lastDot);

            loadedAsset[filename] = *loaded;
        }

        for(auto& asset : loadedAsset){
            for(auto& node : asset.second.get() -> topNodes){
                if (auto meshNode = std::dynamic_pointer_cast<MeshNode>(node)) { 
                    objects[meshNode->mesh->name] = meshNode; 
                }
            }
        }
    }

    void update(float& deltatime);
    void draw(DrawContext& drawContext);
    void set_imgui();



};
