// Contains GLTF loading logic (mesh and materials)
#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

#include "vk_mem_alloc.h"

class VulkanEngine; // Forward declaration

/**
 * Contains buffer for index, buffer for vertex
 * and device address for vertex buffer
 */
struct GPUMeshBuffers
{
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

/**
 * Defines how to treat the material
 * MainColor -> default
 * Transparent -> texture is white
 * Other
 */
enum class MaterialPass : uint8_t
{
    MainColor,
    Transparent,
    Other
};


/**
 * Contains the material pipeline and its layout
 */
struct MaterialPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

/**
 * 
 */
struct MatPipelines
{
    MaterialPipeline transparentPipeline;
    MaterialPipeline opaquePipeline;
    VkDescriptorSetLayout materialLayout;
};



/**
 * A material instance
 * Contains a MaterialPipeline obj
 * A descriptor set
 * And a Material Pass value
 */
struct MaterialInstance
{
    MaterialPipeline * pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

/**
 * Matrial Instance wrapper
 */
struct GLTFMaterial {
    MaterialInstance data;
};

/**
 * Describes the surface of a mesh by defining the materials used
 */
struct GeoSurface{
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;

    Bounds bounds;
};

/**
 * A Mesh obj
 * it has name
 * surfaces
 * And MeshBuffer obj with index and vertex buffer
 */
struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

/**
 * Structures that holds 2 pipelines, one fro transparent draws, the other for opaque
 * It has MatrialConstant structure in it, as well as Material Resources
 */
struct GLTFMetallic_Roughness{
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;
    /**
     * colorFactors are used to multiply the color texture
     * metal_rough_factors has roughness parameters on r and b components
     */
    struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    glm::vec4 extra[14]; // padding for uniform buffers
    };

    struct MaterialResources
    {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    // void build_pipelines(VulkanEngine& engine, std::string&, std::string&);
    void clear_resources(VkDevice device); // TODO

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
    
};

MatPipelines build_pipelines(VulkanEngine& engine, std::string&, std::string&, GLTFMetallic_Roughness& material);

/**
 * Load the meshes from file
 */
std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine& engine, std::filesystem::path filePath);

/**
 * Uploads vetices and indices of a mesh inside the correspective buffers
 */
GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices, VulkanEngine& engine);



struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;
    VkDeviceAddress vertexBufferAddress;

    MaterialInstance * material;

    glm::mat4 transform;

    Bounds bounds;
};

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
    std::vector<RenderObject> transparentSurfaces;
};

class IRenderable {
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};


struct Node : public IRenderable {
    std::weak_ptr<Node> parent; // weak pointer to avoid circular dependencies
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 translateMatrix;
    glm::mat4 scaleMatrix;
    glm::mat4 rotMatrix;
    glm::mat4 worldTransform;

    std::string name;

    void refreshTransform(const glm::mat4& parentMatrix){
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c -> refreshTransform(worldTransform);
        }
    }


    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx){
        for(auto& c : children) {
            c -> Draw(topMatrix, ctx);
        }
    }

    void update(glm::mat4& rot) {
        localTransform = rot * localTransform;

        refreshTransform(glm::mat4(1));
    }
};


struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};







// -------------------------------------- IMPROVED VERSION

struct LoadedGLTF : public IRenderable{
    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that don't have a parent, for iterating thrugh the fiel in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;
    DescriptorAllocatorGrowable descriptorPool;
    AllocatedBuffer materialDataBuffer;

    VulkanEngine* creator;

    ~LoadedGLTF() { clearAll(); };

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx);

    void updateMaterial();

    void updateNodesRotation(glm::mat4 rot) {
        for(auto& n : topNodes){
            n -> update(rot);
        }
    }

private:
    void clearAll();
};


std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine& engine, std::filesystem::path filePath, GLTFMetallic_Roughness& materialObj);

VkFilter extract_filter(fastgltf::Filter filter);

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);

