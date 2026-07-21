//
// Created by Shagu on 06.07.2026.
//

#ifndef SHUTTLEENGINE_SCENEIMPORTER_HPP
#define SHUTTLEENGINE_SCENEIMPORTER_HPP
#include <vector>
#include <filesystem>
#include <assimp/scene.h>
#include <vulkan/vulkan.hpp>

#include "BlobLayout.hpp"
#include "assimp/Importer.hpp"

namespace shuttle_engine::assets{

    class SceneImporter {
    public:
        static bool loadScene(std::string const& inputPath, std::string const& outputPath);

    private:
        // All methods are static; no persistent importer is stored here.

        static void parseSceneGraph(const aiScene *scene, std::vector<format::SceneNode> &outNodes,
                             std::vector<format::NodeLevelRange> &outLevels);

        // compiledTexturePaths может быть дополнен при парсинге — некоторые текстуры могут быть найдены только тут
        static void parseMaterials(
            const aiScene *scene,
            const std::filesystem::path& sourceDir,
            std::vector<std::string> &compiledTexturePaths,
            std::vector<shuttle_engine::format::MaterialInfo> &outMaterials);

        static void parseLights(const aiScene *scene, std::vector<format::DirectionalLight> &outDirLights,
                                std::vector<format::PointLight> &outPointLights,
                                std::vector<format::SpotLight> &outSpotLights);

        static void processGeometry(const aiScene *scene, std::vector<format::MeshHeader> &outMeshes,
                                    std::vector<uint8_t> &globalBulkData);

        static void parseAnimations(const aiScene *scene, const std::vector<format::SceneNode> &compiledNodes,
                                    std::vector<format::SkeletonData> &outSkeletons, std::vector<format::BoneData> &outBones,
                                    std::vector<format::AnimationClip> &outClips,
                                    std::vector<format::BoneChannel> &outBoneChannels,
                                    std::vector<format::MorphTarget> &outMorphTargets,
                                    std::vector<format::MorphChannel> &outMorphChannels,
                                    std::vector<format::MaterialProperty> &outMatProperties,
                                    std::vector<format::MaterialChannel> &outMatChannels,
                                    std::vector<float> &globalKeyframeTimes,
                                    std::vector<format::AnimationKeyframeValue> &globalKeyframeValues);

        static void writeBlob(const std::string &outputPath, const std::vector<format::SceneNode> &nodes,
                              const std::vector<format::NodeLevelRange> &levels,
                              const std::vector<format::MaterialInfo> &materials,
                              const std::vector<format::MeshHeader> &meshes,
                              const std::vector<format::SkeletonData> &skeletons,
                              const std::vector<format::BoneData> &bones, const std::vector<format::AnimationClip> &clips,
                              const std::vector<format::BoneChannel> &boneChannels,
                              const std::vector<format::MorphTarget> &morphTargets,
                              const std::vector<format::MorphChannel> &morphChannels,
                              const std::vector<format::MaterialProperty> &matProperties,
                              const std::vector<format::MaterialChannel> &matChannels,
                              const std::vector<format::DirectionalLight> &dirLights,
                              const std::vector<format::PointLight> &pointLights,
                              const std::vector<format::SpotLight> &spotLights,
                              const std::vector<float> &keyframeTimes,
                              const std::vector<format::AnimationKeyframeValue> &keyframeValues,
                              const std::vector<uint8_t> &geometryBulkData,
                              const std::vector<format::TextureMetaData> &textureMetas,
                              const std::vector<std::vector<uint8_t>> &textureBlobs);
    };
}
#endif //SHUTTLEENGINE_SCENEIMPORTER_HPP
