#include "AssimpSceneImporter.hpp"

#include <assimp/Importer.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>

namespace shuttle::assets::scene_compiler
{
namespace
{
glm::mat4 toGlm(const aiMatrix4x4& matrix)
{
    glm::mat4 result{1.0f};

    result[0][0] = matrix.a1;
    result[1][0] = matrix.a2;
    result[2][0] = matrix.a3;
    result[3][0] = matrix.a4;

    result[0][1] = matrix.b1;
    result[1][1] = matrix.b2;
    result[2][1] = matrix.b3;
    result[3][1] = matrix.b4;

    result[0][2] = matrix.c1;
    result[1][2] = matrix.c2;
    result[2][2] = matrix.c3;
    result[3][2] = matrix.c4;

    result[0][3] = matrix.d1;
    result[1][3] = matrix.d2;
    result[2][3] = matrix.d3;
    result[3][3] = matrix.d4;

    return result;
}

glm::vec3 toGlm(const aiVector3D& value)
{
    return glm::vec3(value.x, value.y, value.z);
}

glm::quat toGlm(const aiQuaternion& value)
{
    return glm::quat(value.w, value.x, value.y, value.z);
}

std::string toString(const aiString& value)
{
    return std::string(value.C_Str());
}

std::filesystem::path normalizePath(const std::filesystem::path& path)
{
    return path.lexically_normal();
}

std::string extensionWithoutDot(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();

    if (!extension.empty() && extension.front() == '.')
    {
        extension.erase(extension.begin());
    }

    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return extension;
}

int32_t parseEmbeddedTextureIndex(const std::string& assimpPath)
{
    if (assimpPath.empty() || assimpPath.front() != '*')
    {
        return InvalidIndexI32;
    }

    try
    {
        return std::stoi(assimpPath.substr(1));
    }
    catch (...)
    {
        return InvalidIndexI32;
    }
}

formats::texture::TextureSemantic semanticFromAssimpType(aiTextureType type)
{
    switch (type)
    {
    case aiTextureType_BASE_COLOR:
    case aiTextureType_DIFFUSE: return formats::texture::TextureSemantic::Albedo;

    case aiTextureType_NORMALS:
    case aiTextureType_NORMAL_CAMERA: return formats::texture::TextureSemantic::Normal;

    case aiTextureType_GLTF_METALLIC_ROUGHNESS: return formats::texture::TextureSemantic::ORM;

    case aiTextureType_EMISSIVE: return formats::texture::TextureSemantic::Emissive;

    default: return formats::texture::TextureSemantic::Unknown;
    }
}

struct ImportContext
{
    const aiScene* assimpScene = nullptr;

    std::filesystem::path sourceDirectory;

    ImportedScene scene;

    std::unordered_map<std::string, int32_t> textureKeyToIndex;
    std::unordered_map<std::string, uint32_t> nodeNameToIndex;
};

int32_t addFileTexture(ImportContext& context, const std::filesystem::path& path,
                       formats::texture::TextureSemantic semantic)
{
    const std::filesystem::path normalized = normalizePath(path);

    const std::string key = "file://" + normalized.string();

    if (const auto it = context.textureKeyToIndex.find(key); it != context.textureKeyToIndex.end())
    {
        return it->second;
    }

    ImportedTexture texture{};
    texture.name = normalized.filename().string();

    texture.sourceKind = ImportedTextureSourceKind::File;

    texture.sourcePath = normalized;

    texture.formatHint = extensionWithoutDot(normalized);

    texture.semantic = semantic;

    const int32_t index = static_cast<int32_t>(context.scene.textures.size());

    context.scene.textures.push_back(std::move(texture));

    context.textureKeyToIndex[key] = index;

    return index;
}

int32_t addEmbeddedTexture(ImportContext& context, int32_t embeddedIndex, formats::texture::TextureSemantic semantic)
{
    if (!context.assimpScene || embeddedIndex < 0 ||
        static_cast<uint32_t>(embeddedIndex) >= context.assimpScene->mNumTextures)
    {
        return InvalidIndexI32;
    }

    const std::string key = "embedded://" + std::to_string(embeddedIndex);

    if (const auto it = context.textureKeyToIndex.find(key); it != context.textureKeyToIndex.end())
    {
        return it->second;
    }

    const aiTexture* aiTexture = context.assimpScene->mTextures[embeddedIndex];

    if (!aiTexture || !aiTexture->pcData)
    {
        return InvalidIndexI32;
    }

    ImportedTexture texture{};
    texture.name = aiTexture->mFilename.length > 0 ? std::string(aiTexture->mFilename.C_Str()) : key;

    texture.semantic = semantic;

    if (aiTexture->mHeight == 0)
    {
        texture.sourceKind = ImportedTextureSourceKind::EmbeddedEncoded;

        texture.embeddedBytes.resize(aiTexture->mWidth);

        std::memcpy(texture.embeddedBytes.data(), aiTexture->pcData, aiTexture->mWidth);

        texture.formatHint = aiTexture->achFormatHint[0] != '\0' ? std::string(aiTexture->achFormatHint)
                                                                 : extensionWithoutDot(texture.name);
    }
    else
    {
        texture.sourceKind = ImportedTextureSourceKind::EmbeddedRGBA;

        texture.width = aiTexture->mWidth;

        texture.height = aiTexture->mHeight;

        const size_t byteSize =
            static_cast<size_t>(aiTexture->mWidth) * static_cast<size_t>(aiTexture->mHeight) * sizeof(aiTexel);

        texture.embeddedBytes.resize(byteSize);

        std::memcpy(texture.embeddedBytes.data(), aiTexture->pcData, byteSize);

        texture.formatHint = "rgba8";
    }

    const int32_t index = static_cast<int32_t>(context.scene.textures.size());

    context.scene.textures.push_back(std::move(texture));

    context.textureKeyToIndex[key] = index;

    return index;
}

int32_t importMaterialTexture(ImportContext& context, const aiMaterial* material, aiTextureType textureType)
{
    if (!material || material->GetTextureCount(textureType) == 0)
    {
        return InvalidIndexI32;
    }

    aiString path;

    if (material->GetTexture(textureType, 0, &path) != AI_SUCCESS)
    {
        return InvalidIndexI32;
    }

    const std::string pathString = path.C_Str();

    if (pathString.empty())
    {
        return InvalidIndexI32;
    }

    const formats::texture::TextureSemantic semantic = semanticFromAssimpType(textureType);

    if (pathString.front() == '*')
    {
        const int32_t embeddedIndex = parseEmbeddedTextureIndex(pathString);

        return addEmbeddedTexture(context, embeddedIndex, semantic);
    }

    std::filesystem::path texturePath = pathString;

    if (texturePath.is_relative())
    {
        texturePath = context.sourceDirectory / texturePath;
    }

    return addFileTexture(context, texturePath, semantic);
}

void importMaterials(ImportContext& context)
{
    const aiScene* scene = context.assimpScene;

    if (!scene)
    {
        return;
    }

    context.scene.materials.reserve(scene->mNumMaterials);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* material = scene->mMaterials[i];

        ImportedMaterial imported{};

        aiString materialName;

        if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS)
        {
            imported.name = materialName.C_Str();
        }

        aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);

        aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);

        material->Get(AI_MATKEY_BASE_COLOR, baseColor);

        material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

        imported.baseColorFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);

        imported.emissiveFactor = glm::vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f);

        material->Get(AI_MATKEY_METALLIC_FACTOR, imported.metallicFactor);

        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, imported.roughnessFactor);

        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, imported.alphaCutoff);

        material->Get("$mat.gltf.occlusionStrength", 0, 0, imported.occlusionStrength);

        material->Get(AI_MATKEY_EMISSIVE_INTENSITY, imported.emissiveStrength);

        aiString alphaMode;

        if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
        {
            const std::string mode = alphaMode.C_Str();

            if (mode == "MASK")
            {
                imported.alphaMode = ImportedAlphaMode::Mask;
            }
            else if (mode == "BLEND")
            {
                imported.alphaMode = ImportedAlphaMode::Blend;
            }
        }

        bool twoSided = false;

        if (material->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
        {
            imported.doubleSided = twoSided;
        }

        imported.albedoTexture = importMaterialTexture(context, material, aiTextureType_BASE_COLOR);

        if (imported.albedoTexture == InvalidIndexI32)
        {
            imported.albedoTexture = importMaterialTexture(context, material, aiTextureType_DIFFUSE);
        }

        imported.normalTexture = importMaterialTexture(context, material, aiTextureType_NORMAL_CAMERA);

        if (imported.normalTexture == InvalidIndexI32)
        {
            imported.normalTexture = importMaterialTexture(context, material, aiTextureType_NORMALS);
        }

        imported.ormTexture = importMaterialTexture(context, material, aiTextureType_GLTF_METALLIC_ROUGHNESS);

        imported.occlusionTexture = importMaterialTexture(context, material, aiTextureType_AMBIENT_OCCLUSION);

        imported.roughnessTexture = importMaterialTexture(context, material, aiTextureType_DIFFUSE_ROUGHNESS);

        imported.metallicTexture = importMaterialTexture(context, material, aiTextureType_METALNESS);

        imported.emissiveTexture = importMaterialTexture(context, material, aiTextureType_EMISSIVE);

        context.scene.materials.push_back(std::move(imported));
    }
}

int32_t importNodeRecursive(ImportContext& context, const aiNode* aiNode, int32_t parentIndex)
{
    ImportedNode node{};
    node.name = aiNode->mName.C_Str();

    node.parent = parentIndex;

    node.localTransform = toGlm(aiNode->mTransformation);

    node.meshes.reserve(aiNode->mNumMeshes);

    for (uint32_t i = 0; i < aiNode->mNumMeshes; ++i)
    {
        node.meshes.push_back(static_cast<int32_t>(aiNode->mMeshes[i]));
    }

    const int32_t nodeIndex = static_cast<int32_t>(context.scene.nodes.size());

    context.nodeNameToIndex[node.name] = static_cast<uint32_t>(nodeIndex);

    context.scene.nodes.push_back(std::move(node));

    for (uint32_t i = 0; i < aiNode->mNumChildren; ++i)
    {
        const int32_t childIndex = importNodeRecursive(context, aiNode->mChildren[i], nodeIndex);

        context.scene.nodes[nodeIndex].children.push_back(childIndex);
    }

    return nodeIndex;
}

    void importNodes(ImportContext& context)
{
    if (!context.assimpScene || !context.assimpScene->mRootNode)
    {
        return;
    }

    struct QueueEntry
    {
        const aiNode* aiNode = nullptr;
        int32_t parentIndex = InvalidIndexI32;
    };

    std::queue<QueueEntry> queue;

    queue.push({
        .aiNode = context.assimpScene->mRootNode,
        .parentIndex = InvalidIndexI32
    });

    while (!queue.empty())
    {
        QueueEntry entry = queue.front();
        queue.pop();

        ImportedNode node{};
        node.name = entry.aiNode->mName.C_Str();
        node.parent = entry.parentIndex;
        node.localTransform = toGlm(entry.aiNode->mTransformation);

        node.meshes.reserve(entry.aiNode->mNumMeshes);

        for (uint32_t i = 0; i < entry.aiNode->mNumMeshes; ++i)
        {
            node.meshes.push_back(
                static_cast<int32_t>(entry.aiNode->mMeshes[i]));
        }

        const int32_t nodeIndex =
            static_cast<int32_t>(context.scene.nodes.size());

        context.nodeNameToIndex[node.name] =
            static_cast<uint32_t>(nodeIndex);

        context.scene.nodes.push_back(std::move(node));

        //
        // Добавляем себя в children родителя
        //
        if (entry.parentIndex >= 0)
        {
            context.scene.nodes[entry.parentIndex]
                .children.push_back(nodeIndex);
        }

        //
        // Кладём детей в очередь
        //
        for (uint32_t i = 0; i < entry.aiNode->mNumChildren; ++i)
        {
            queue.push({
                .aiNode = entry.aiNode->mChildren[i],
                .parentIndex = nodeIndex
            });
        }
    }
}

void addBoneInfluence(VertexBoneInfluence& influence, uint16_t boneIndex, float weight)
{
    for (size_t i = 0; i < 4; ++i)
    {
        if (influence.boneWeights[i] == 0.0f)
        {
            influence.boneIndices[i] = boneIndex;

            influence.boneWeights[i] = weight;

            return;
        }
    }

    size_t weakestIndex = 0;

    for (size_t i = 1; i < 4; ++i)
    {
        if (influence.boneWeights[i] < influence.boneWeights[weakestIndex])
        {
            weakestIndex = i;
        }
    }

    if (weight > influence.boneWeights[weakestIndex])
    {
        influence.boneIndices[weakestIndex] = boneIndex;

        influence.boneWeights[weakestIndex] = weight;
    }
}

void normalizeBoneInfluences(VertexBoneInfluence& influence)
{
    float sum = 0.0f;

    for (float weight : influence.boneWeights)
    {
        sum += weight;
    }

    if (sum <= 0.0f)
    {
        return;
    }

    for (float& weight : influence.boneWeights)
    {
        weight /= sum;
    }
}

void importMeshes(ImportContext& context)
{
    const aiScene* scene = context.assimpScene;

    if (!scene)
    {
        return;
    }

    context.scene.meshes.reserve(scene->mNumMeshes);

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* aiMesh = scene->mMeshes[meshIndex];

        ImportedMesh mesh{};
        mesh.name = aiMesh->mName.C_Str();

        mesh.materialIndex = static_cast<int32_t>(aiMesh->mMaterialIndex);

        mesh.vertices.resize(aiMesh->mNumVertices);

        for (uint32_t v = 0; v < aiMesh->mNumVertices; ++v)
        {
            ImportedVertex vertex{};

            vertex.position = glm::vec3(aiMesh->mVertices[v].x, aiMesh->mVertices[v].y, aiMesh->mVertices[v].z);

            if (aiMesh->HasNormals())
            {
                vertex.normal = glm::vec3(aiMesh->mNormals[v].x, aiMesh->mNormals[v].y, aiMesh->mNormals[v].z);
            }

            if (aiMesh->HasTangentsAndBitangents())
            {
                vertex.tangent =
                    glm::vec4(aiMesh->mTangents[v].x, aiMesh->mTangents[v].y, aiMesh->mTangents[v].z, 1.0f);
            }

            if (aiMesh->HasTextureCoords(0))
            {
                vertex.texCoord0 = glm::vec2(aiMesh->mTextureCoords[0][v].x, aiMesh->mTextureCoords[0][v].y);
            }

            if (aiMesh->HasVertexColors(0))
            {
                vertex.color = glm::vec4(aiMesh->mColors[0][v].r, aiMesh->mColors[0][v].g, aiMesh->mColors[0][v].b,
                                         aiMesh->mColors[0][v].a);
            }

            mesh.vertices[v] = vertex;
        }

        for (uint32_t f = 0; f < aiMesh->mNumFaces; ++f)
        {
            const aiFace& face = aiMesh->mFaces[f];

            if (face.mNumIndices != 3)
            {
                continue;
            }

            mesh.indices.push_back(face.mIndices[0]);

            mesh.indices.push_back(face.mIndices[1]);

            mesh.indices.push_back(face.mIndices[2]);
        }

        if (aiMesh->HasBones())
        {
            ImportedSkin skin{};
            skin.name = mesh.name + "_Skin";

            mesh.skinning.resize(aiMesh->mNumVertices);

            skin.bones.reserve(aiMesh->mNumBones);

            for (uint32_t boneIndex = 0; boneIndex < aiMesh->mNumBones; ++boneIndex)
            {
                const aiBone* aiBone = aiMesh->mBones[boneIndex];

                ImportedBone bone{};
                bone.name = aiBone->mName.C_Str();

                bone.inverseBindMatrix = toGlm(aiBone->mOffsetMatrix);

                if (const auto it = context.nodeNameToIndex.find(bone.name); it != context.nodeNameToIndex.end())
                {
                    bone.nodeIndex = it->second;
                }

                for (uint32_t weightIndex = 0; weightIndex < aiBone->mNumWeights; ++weightIndex)
                {
                    const aiVertexWeight& weight = aiBone->mWeights[weightIndex];

                    if (weight.mVertexId >= mesh.skinning.size())
                    {
                        continue;
                    }

                    addBoneInfluence(mesh.skinning[weight.mVertexId], static_cast<uint16_t>(boneIndex), weight.mWeight);
                }

                skin.bones.push_back(bone);
            }

            for (VertexBoneInfluence& influence : mesh.skinning)
            {
                normalizeBoneInfluences(influence);
            }

            for (uint32_t boneIndex = 0; boneIndex < skin.bones.size(); ++boneIndex)
            {
                ImportedBone& bone = skin.bones[boneIndex];

                if (bone.nodeIndex == InvalidIndexU32)
                {
                    continue;
                }

                const ImportedNode& node = context.scene.nodes[bone.nodeIndex];

                if (node.parent == InvalidIndexI32)
                {
                    continue;
                }

                for (uint32_t parentBoneIndex = 0; parentBoneIndex < skin.bones.size(); ++parentBoneIndex)
                {
                    if (skin.bones[parentBoneIndex].nodeIndex == static_cast<uint32_t>(node.parent))
                    {
                        bone.parentBone = parentBoneIndex;

                        break;
                    }
                }
            }

            for (uint32_t boneIndex = 0; boneIndex < skin.bones.size(); ++boneIndex)
            {
                if (skin.bones[boneIndex].parentBone == InvalidIndexU32)
                {
                    skin.skeletonRoot = boneIndex;

                    break;
                }
            }

            mesh.skinIndex = static_cast<int32_t>(context.scene.skins.size());

            context.scene.skins.push_back(std::move(skin));
        }

        if (aiMesh->mNumAnimMeshes > 0)
        {
            mesh.morphTargets.reserve(aiMesh->mNumAnimMeshes);

            for (uint32_t morphIndex = 0; morphIndex < aiMesh->mNumAnimMeshes; ++morphIndex)
            {
                const aiAnimMesh* aiMorph = aiMesh->mAnimMeshes[morphIndex];

                ImportedMorphTarget morph{};
                morph.name = aiMorph->mName.C_Str();

                for (uint32_t v = 0; v < aiMesh->mNumVertices; ++v)
                {
                    ImportedMorphVertexDelta delta{};
                    delta.vertexIndex = v;

                    if (aiMorph->HasPositions())
                    {
                        const glm::vec3 base =
                            glm::vec3(aiMesh->mVertices[v].x, aiMesh->mVertices[v].y, aiMesh->mVertices[v].z);

                        const glm::vec3 target =
                            glm::vec3(aiMorph->mVertices[v].x, aiMorph->mVertices[v].y, aiMorph->mVertices[v].z);

                        delta.positionDelta = target - base;

                        morph.maxPositionDelta = glm::max(morph.maxPositionDelta, glm::abs(delta.positionDelta));
                    }

                    if (aiMorph->HasNormals() && aiMesh->HasNormals())
                    {
                        const glm::vec3 base =
                            glm::vec3(aiMesh->mNormals[v].x, aiMesh->mNormals[v].y, aiMesh->mNormals[v].z);

                        const glm::vec3 target =
                            glm::vec3(aiMorph->mNormals[v].x, aiMorph->mNormals[v].y, aiMorph->mNormals[v].z);

                        delta.normalDelta = target - base;
                    }

                    if (delta.positionDelta != glm::vec3(0.0f) || delta.normalDelta != glm::vec3(0.0f))
                    {
                        morph.deltas.push_back(delta);
                    }
                }

                mesh.morphTargets.push_back(std::move(morph));
            }
        }

        context.scene.meshes.push_back(std::move(mesh));
    }
}

void importLights(ImportContext& context)
{
    const aiScene* scene = context.assimpScene;

    if (!scene)
    {
        return;
    }

    context.scene.lights.reserve(scene->mNumLights);

    for (uint32_t i = 0; i < scene->mNumLights; ++i)
    {
        const aiLight* aiLight = scene->mLights[i];

        ImportedLight light{};
        light.name = aiLight->mName.C_Str();

        light.color = glm::vec3(aiLight->mColorDiffuse.r, aiLight->mColorDiffuse.g, aiLight->mColorDiffuse.b);

        light.intensity = 1.0f;

        switch (aiLight->mType)
        {
        case aiLightSource_DIRECTIONAL: light.type = ImportedLightType::Directional; break;

        case aiLightSource_POINT:
            light.type = ImportedLightType::Point;

            light.range = aiLight->mAttenuationLinear > 0.0f ? 1.0f / aiLight->mAttenuationLinear : 10.0f;
            break;

        case aiLightSource_SPOT:
            light.type = ImportedLightType::Spot;

            light.range = aiLight->mAttenuationLinear > 0.0f ? 1.0f / aiLight->mAttenuationLinear : 15.0f;

            light.innerConeAngle = aiLight->mAngleInnerCone;

            light.outerConeAngle = aiLight->mAngleOuterCone;
            break;

        default: continue;
        }

        const int32_t lightIndex = static_cast<int32_t>(context.scene.lights.size());

        context.scene.lights.push_back(light);

        if (const auto it = context.nodeNameToIndex.find(light.name); it != context.nodeNameToIndex.end())
        {
            context.scene.nodes[it->second].lightIndex = lightIndex;
        }
    }
}

ImportedInterpolationMode defaultInterpolation()
{
    return ImportedInterpolationMode::Linear;
}

void importAnimations(ImportContext& context)
{
    const aiScene* scene = context.assimpScene;

    if (!scene)
    {
        return;
    }

    context.scene.animations.reserve(scene->mNumAnimations);

    for (uint32_t animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex)
    {
        const aiAnimation* aiAnimation = scene->mAnimations[animationIndex];

        ImportedAnimation animation{};
        animation.name = aiAnimation->mName.C_Str();

        animation.ticksPerSecond = aiAnimation->mTicksPerSecond > 0.0 ? aiAnimation->mTicksPerSecond : 1.0;

        animation.duration = aiAnimation->mDuration / animation.ticksPerSecond;

        animation.startTime = 0.0;

        animation.endTime = animation.duration;

        animation.channels.reserve(aiAnimation->mNumChannels);

        for (uint32_t channelIndex = 0; channelIndex < aiAnimation->mNumChannels; ++channelIndex)
        {
            const aiNodeAnim* aiChannel = aiAnimation->mChannels[channelIndex];

            const std::string nodeName = aiChannel->mNodeName.C_Str();

            auto nodeIt = context.nodeNameToIndex.find(nodeName);

            if (nodeIt == context.nodeNameToIndex.end())
            {
                continue;
            }

            ImportedAnimationChannel channel{};
            channel.nodeIndex = nodeIt->second;

            channel.translationInterpolation = defaultInterpolation();

            channel.rotationInterpolation = defaultInterpolation();

            channel.scaleInterpolation = defaultInterpolation();

            channel.positions.reserve(aiChannel->mNumPositionKeys);

            for (uint32_t keyIndex = 0; keyIndex < aiChannel->mNumPositionKeys; ++keyIndex)
            {
                const aiVectorKey& key = aiChannel->mPositionKeys[keyIndex];

                PositionKey position{};
                position.time = key.mTime / animation.ticksPerSecond;

                position.value = toGlm(key.mValue);

                channel.positions.push_back(position);
            }

            channel.rotations.reserve(aiChannel->mNumRotationKeys);

            for (uint32_t keyIndex = 0; keyIndex < aiChannel->mNumRotationKeys; ++keyIndex)
            {
                const aiQuatKey& key = aiChannel->mRotationKeys[keyIndex];

                RotationKey rotation{};
                rotation.time = key.mTime / animation.ticksPerSecond;

                rotation.value = toGlm(key.mValue);

                channel.rotations.push_back(rotation);
            }

            channel.scales.reserve(aiChannel->mNumScalingKeys);

            for (uint32_t keyIndex = 0; keyIndex < aiChannel->mNumScalingKeys; ++keyIndex)
            {
                const aiVectorKey& key = aiChannel->mScalingKeys[keyIndex];

                ScaleKey scale{};
                scale.time = key.mTime / animation.ticksPerSecond;

                scale.value = toGlm(key.mValue);

                channel.scales.push_back(scale);
            }

            animation.channels.push_back(std::move(channel));
        }

        for (uint32_t meshChannelIndex = 0; meshChannelIndex < aiAnimation->mNumMeshChannels; ++meshChannelIndex)
        {
            const aiMeshAnim* aiMeshChannel = aiAnimation->mMeshChannels[meshChannelIndex];

            ImportedAnimationChannel channel{};
            channel.nodeIndex = InvalidIndexU32;

            channel.weightInterpolation = ImportedInterpolationMode::Linear;

            channel.weights.reserve(aiMeshChannel->mNumKeys);

            for (uint32_t keyIndex = 0; keyIndex < aiMeshChannel->mNumKeys; ++keyIndex)
            {
                const aiMeshKey& key = aiMeshChannel->mKeys[keyIndex];

                WeightKey weight{};
                weight.time = key.mTime / animation.ticksPerSecond;

                weight.value.push_back(static_cast<float>(key.mValue));

                channel.weights.push_back(std::move(weight));
            }

            animation.channels.push_back(std::move(channel));
        }

        context.scene.animations.push_back(std::move(animation));
    }
}
} // namespace

std::optional<ImportedScene> AssimpSceneImporter::import(const std::filesystem::path& filePath)
{
    Assimp::Importer importer;

    const unsigned int flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace |
                               aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_FlipUVs;

    const aiScene* scene = importer.ReadFile(filePath.string(), flags);

    if (!scene)
    {
        std::cerr << "[AssimpSceneImporter] Failed to import scene: " << importer.GetErrorString() << std::endl;

        return std::nullopt;
    }

    ImportContext context{};
    context.assimpScene = scene;

    context.sourceDirectory = std::filesystem::absolute(filePath).parent_path();

    importNodes(context);
    importMaterials(context);
    importMeshes(context);
    importLights(context);
    importAnimations(context);

    return std::move(context.scene);
}
} // namespace shuttle::assets::scene_compiler