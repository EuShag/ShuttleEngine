//
// Created by Shagu on 06.07.2026.
//

#include "SceneImporter.hpp"

#include <fstream>
#include <iostream>
#include <queue>
#include <array>
#include <glm/gtx/quaternion.hpp>
#include <cstddef>
#include <functional>
#include <cctype>

#include "BlobLayout.hpp"
#include "meshoptimizer.h"
#include "assimp/GltfMaterial.h"
#include "assimp/postprocess.h"
#include "../TextureImporter/TextureImporter.hpp"
#include <filesystem>
#include <omp.h>

namespace shuttle_engine::assets {

    // =============================================================================
    // FNV-1a хэширование для имён нод и клипов
    // =============================================================================
    static uint32_t fnv1a_hash(const std::string& str) {
        constexpr uint32_t FNV_PRIME = 16777619u;
        constexpr uint32_t FNV_BASIS = 2166136261u;

        uint32_t hash = FNV_BASIS;
        for (unsigned char c : str) {
            hash ^= c;
            hash *= FNV_PRIME;
        }
        return hash;
    }

void SceneImporter::parseSceneGraph(
    const aiScene* scene,
    std::vector<format::SceneNode>& outNodes,
    std::vector<format::NodeLevelRange>& outLevels
) {
    if (!scene || !scene->mRootNode) return;

    // =========================================================================
    // ЭТАП 1: АВТОМАТИЧЕСКОЕ ОПРЕДЕЛЕНИЕ ДИНАМИЧЕСКИХ НОД
    // =========================================================================
    std::unordered_set<std::string> animatedNodeNames;
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        aiAnimation* anim = scene->mAnimations[i];
        for (uint32_t j = 0; j < anim->mNumChannels; ++j) {
            aiNodeAnim* channel = anim->mChannels[j];
            animatedNodeNames.insert(channel->mNodeName.C_Str());
        }
    }

    // =========================================================================
    // ЭТАП 2: ПОУРОВНЕВЫЙ BFS ОБХОД С УМНЫМ СХЛОПЫВАНИЕМ (NODE COLLAPSING)
    // =========================================================================
    struct BfsItem {
        aiNode*   assimpNode;
        uint32_t  accumulatedParentIdx; // Валидный ID родителя в итоговом SSBO буфере на GPU
        glm::vec3 accTranslation;       // Накопленный сдвиг от схлопнутых пустых родителей
        glm::quat accRotation;          // Накопленное вращение от схлопнутых пустых родителей
        glm::vec3 accScale;             // Накопленный масштаб от схлопнутых пустых родителей
    };

    std::queue<BfsItem> currentLevelQueue;
    std::queue<BfsItem> nextLevelQueue;

    // Корень сцены всегда инициализирует базис (Уровень 0), его родитель — 0xFFFFFFFF
    currentLevelQueue.push({
        .assimpNode = scene->mRootNode, .accumulatedParentIdx = 0xFFFFFFFF,
        .accTranslation = glm::vec3(0.0f), .accRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f), .accScale = glm::vec3(1.0f)
    });

    outNodes.reserve(2000);
    outLevels.reserve(16);

    while (!currentLevelQueue.empty()) {
        format::NodeLevelRange levelRange{};
        levelRange.startNodeIdx = static_cast<uint32_t>(outNodes.size());

        uint32_t nodesAddedOnThisLevel = 0;

        // Посещаем все ноды, находящиеся строго на текущем уровне глубины
        while (!currentLevelQueue.empty()) {
            BfsItem current = currentLevelQueue.front();
            currentLevelQueue.pop();

            aiNode* aNode = current.assimpNode;
            std::string nodeName(aNode->mName.C_Str());

            // Извлекаем локальные TRS компоненты из матрицы Assimp
            aiVector3D localScale;
            aiQuaternion localRot;
            aiVector3D localTrans;
            aNode->mTransformation.Decompose(localScale, localRot, localTrans);

            glm::vec3 lT(localTrans.x, localTrans.y, localTrans.z);
            glm::quat lR(localRot.w, localRot.x, localRot.y, localRot.z);
            glm::vec3 lS(localScale.x, localScale.y, localScale.z);

            // Каскадно перемножаем локальную матрицу с накопленной трансформацией родителей
            glm::vec3 finalScale       = current.accScale * lS;
            glm::quat finalRotation    = current.accRotation * lR;
            glm::vec3 finalTranslation = current.accTranslation + (current.accRotation * (current.accScale * lT));

            // ЖЕСТКИЕ ПРАВИЛА СОХРАНЕНИЯ НОДЫ:
            // 1. Обязательно оставляем корень сцены (индекс 0)
            // 2. Оставляем, если у ноды есть реальный меш для отрисовки
            // 3. Оставляем, если нода анимирована (даже если у нее пока нет меша, она двигает детей!)
            bool isAnimated = animatedNodeNames.contains(nodeName);
            bool isKeepNode = aNode->mNumMeshes > 0 || aNode == scene->mRootNode || isAnimated;
            if (aNode->mNumMeshes > 1) {
                std::cout << "Здесь " << aNode->mNumMeshes << " meshes" << std::endl;
            }

            uint32_t nextParentIdxForChildren = current.accumulatedParentIdx;

                if (isKeepNode) {
                    auto currentIdx = static_cast<uint32_t>(outNodes.size());

                    for (uint32_t i = 0; i < aNode->mNumMeshes; i++) {
                        format::SceneNode myNode{};
                        myNode.localTranslation  = finalTranslation;
                        myNode.localRotationQuat = glm::vec4(finalRotation.x, finalRotation.y, finalRotation.z, finalRotation.w);
                        myNode.localScale        = finalScale;
                        myNode.parentIndex       = current.accumulatedParentIdx;

                        // instanceIndex: индекс первого меша, либо INVALID_INDEX_U32 для чистых анимированных пустышек-костей
                        myNode.instanceIndex     = aNode->mNumMeshes > 0 ? aNode->mMeshes[i] : format::INVALID_INDEX_U32;

                        // Вычисляем FNV-1a хэш имени ноды для последующего сопоставления с костями
                        myNode.nodeNameHash = fnv1a_hash(nodeName);

                        outNodes.push_back(myNode);
                        nodesAddedOnThisLevel++;

                        // Фиксируем эту ноду как официального родителя для последующих поколений детей
                        nextParentIdxForChildren = currentIdx;
                    }
                    // Сбрасываем накопленные трансформации для детей, так как мы их только что применили
                    finalTranslation = glm::vec3(0.0f);
                    finalRotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    finalScale       = glm::vec3(1.0f);
                }

            // Добавляем всех детей в очередь следующего уровня
            for (uint32_t i = 0; i < aNode->mNumChildren; ++i) {
                nextLevelQueue.push({
                    aNode->mChildren[i], nextParentIdxForChildren,
                    finalTranslation, finalRotation, finalScale
                });
            }
        }

        // Если на этом уровне глубины были физически созданы ноды для GPU — фиксируем диапазон
        if (nodesAddedOnThisLevel > 0) {
            levelRange.nodeCount = nodesAddedOnThisLevel;
            outLevels.push_back(levelRange);
        }

        // Переходим на следующий уровень
        std::swap(currentLevelQueue, nextLevelQueue);
    }

    std::cout << "[AssetProcessor] Ультимативный граф сцены построен успешно!" << std::endl;
    std::cout << "  -> Всего реальных нод упаковано: " << outNodes.size() << std::endl;
    std::cout << "  -> Выделено уровней для GPU-диспатча: " << outLevels.size() << std::endl;
}

void SceneImporter::parseMaterials(
    const aiScene* scene,
    const std::filesystem::path& sourceDir,
    std::vector<std::string>& compiledTexturePaths, // Список уже учтенных путей текстур DDS (может дополняться)
    std::vector<format::MaterialInfo>& outMaterials
) {
    if (!scene || scene->mNumMaterials == 0) return;

    outMaterials.reserve(scene->mNumMaterials);

    // Предварительно создаем хеш-таблицу для быстрого поиска текстур O(1) вместо O(n)
    std::unordered_map<std::string, int32_t> texturePathToIndex;
    for (size_t i = 0; i < compiledTexturePaths.size(); ++i) {
        texturePathToIndex[compiledTexturePaths[i]] = static_cast<int32_t>(i);
    }

    auto toLowerStr = [](std::string s) {
        for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };

    struct TextureCatalogItem {
        std::string path;
        std::string lowerName;
    };
    std::vector<TextureCatalogItem> textureCatalog;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = toLowerStr(entry.path().extension().string());
            if (ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") {
                textureCatalog.push_back({
                    entry.path().lexically_normal().string(),
                    toLowerStr(entry.path().filename().string())
                });
            }
        }
    } catch (...) {
    }

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* aiMat = scene->mMaterials[i];
        shuttle_engine::format::MaterialInfo myMat{};

        // 1. Инициализируем дефолтные значения факторов PBR (на случай, если их нет в файле)
        aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
        aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);  // Используем vec4 чтобы сохранить альфа
        float metallic = 0.0f;
        float roughness = 1.0f;
        float alphaCutoff = 0.5f;
        float emissiveStrength = 1.0f;  // glTF default
        float occlusionStrength = 1.0f;

        // Вытягиваем свойства методами Assimp
        if (aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) != AI_SUCCESS) {
            aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor);
        }
        aiReturn metalicFactorReturn = aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        if (metalicFactorReturn == aiReturn_FAILURE) {
            std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа Metallic Factor, используем 0.0f по умолчанию." << std::endl;
            metallic = 0.0f;
        }

        aiReturn roughnessFactorReturn = aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        if (roughnessFactorReturn == aiReturn_FAILURE) {
            std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа Roughness Factor, используем 1.0f по умолчанию." << std::endl;
            roughness = 1.0f;
        }

        aiReturn alphaCutoffReturn = aiMat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
        if (alphaCutoffReturn == aiReturn_FAILURE) {
            std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа Alpha Cutoff, используем 0.5f по умолчанию." << std::endl;
            alphaCutoff = 0.5f;
        }

        aiReturn emissiveIntensityReturn = aiMat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength);
        if (emissiveIntensityReturn == aiReturn_FAILURE) {
            std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа Emissive Intensity, используем 1.0f по умолчанию." << std::endl;
            // Проверяем альтернативный внутренний ключ Assimp для FBX
            if (aiMat->Get("$mat.emissiveIntensity", 0, 0, emissiveStrength) != AI_SUCCESS) {
                std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа внутреннего Emissive Intensity, используем 1.0f по умолчанию." << std::endl;
                emissiveStrength = 1.0f;
            }
        }

        aiReturn occlusionStrengthReturn = aiMat->Get(AI_MATKEY_SHININESS_STRENGTH, occlusionStrength);
        if (occlusionStrengthReturn == aiReturn_FAILURE) {
            std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа Occlusion Strength, используем 1.0f по умолчанию." << std::endl;
            // Если FBX-фоллбек не найден, проверяем честный glTF Occlusion Strength
            if (aiMat->Get("$mat.gltf.occlusionStrength", 0, 0, occlusionStrength) != AI_SUCCESS) {
                std::cout << "[SceneImporter] Внимание: Материал " << i << " не имеет ключа glTF Occlusion Strength, используем 1.0f по умолчанию." << std::endl;
                occlusionStrength = 1.0f; // По умолчанию затенения в щелях нет
            }
        }

        // Валидируем диапазоны значений PBR
        metallic = glm::clamp(metallic, 0.0f, 1.0f);
        roughness = glm::clamp(roughness, 0.001f, 1.0f);  // Minmax: > 0 для избежания деления на ноль в шейдере
        alphaCutoff = glm::clamp(alphaCutoff, 0.0f, 1.0f);
        emissiveStrength = glm::clamp(emissiveStrength, 0.0f, 1e4f);  // HDR допускает большие значения
        occlusionStrength = glm::clamp(occlusionStrength, 0.0f, 1.0f);

        myMat.baseColorFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
        myMat.emissiveFactor  = glm::vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, emissiveColor.a);  // Сохраняем alpha
        myMat.metallicFactor  = metallic;
        myMat.roughnessFactor = roughness;
        myMat.alphaCutoff     = alphaCutoff;
        myMat.occlusionStrength = occlusionStrength;
        myMat.emissiveStrength  = emissiveStrength;

        // 2. Лямбда-функция для поиска индекса текстуры в глобальном Bindless-списке (теперь O(1)),
        //    с несколькими fallback-стратегиями: точный путь, сравнение по имени файла, проверка встроенных текстур.
        auto normalizePath = [](std::string s) {
            for (auto &c : s) if (c == '\\') c = '/';
            // Убираем префикс ./
            if (s.rfind("./", 0) == 0) s = s.substr(2);
            return s;
        };
        auto filenameFromPath = [](const std::string &p) -> std::string {
            size_t pos = p.find_last_of("/\\");
            return (pos == std::string::npos) ? p : p.substr(pos + 1);
        };

        auto getTextureIndex = [&](aiTextureType type, unsigned int slot = 0) -> int32_t {
            aiString path;
            if (aiMat->GetTexture(type, slot, &path) == AI_SUCCESS) {
                std::string pathStr = path.C_Str();
                std::string norm = normalizePath(pathStr);

                // 1) Точный поиск по нормализованному пути
                auto it = texturePathToIndex.find(norm);
                if (it != texturePathToIndex.end()) return it->second;

                // 2) Поиск по имени файла (в папках пути могут отличаться)
                std::string name = filenameFromPath(norm);
                for (size_t ti = 0; ti < compiledTexturePaths.size(); ++ti) {
                    if (filenameFromPath(normalizePath(compiledTexturePaths[ti])) == name) {
                        return static_cast<int32_t>(ti);
                    }
                }

                // 3) Если путь действительно существует на диске — авто-добавляем его в compiledTexturePaths и возвращаем индекс
                try {
                    if (!norm.empty() && std::filesystem::exists(norm)) {
                        auto newIdx = static_cast<int32_t>(compiledTexturePaths.size());
                        compiledTexturePaths.push_back(norm);
                        texturePathToIndex[norm] = newIdx;
                        return newIdx;
                    }

                    if (!norm.empty()) {
                        std::filesystem::path resolved = sourceDir / std::filesystem::path(norm);
                        if (std::filesystem::exists(resolved)) {
                            std::string resolvedStr = resolved.lexically_normal().string();
                            auto newIdx = static_cast<int32_t>(compiledTexturePaths.size());
                            compiledTexturePaths.push_back(resolvedStr);
                            texturePathToIndex[resolvedStr] = newIdx;
                            texturePathToIndex[norm] = newIdx;
                            return newIdx;
                        }
                    }
                } catch (...) {
                    // filesystem may throw on weird inputs; ignore and continue to other fallbacks
                }

                // 4) Встроенные текстуры Assimp (путь вида "*0" или "*1")
                if (!pathStr.empty() && pathStr[0] == '*') {
                    try {
                        int embIndex = std::stoi(pathStr.substr(1));
                                            if (embIndex >= 0 && static_cast<uint32_t>(embIndex) < scene->mNumTextures) {
                                                // Регистрируем плейсхолдер для встроенной текстуры, чтобы остальной пайплайн мог на неё сослаться.
                                                std::string const embeddedName = std::string("embedded://") + std::to_string(embIndex);
                                                auto const newIdx = static_cast<int32_t>(compiledTexturePaths.size());
                                                compiledTexturePaths.push_back(embeddedName);
                                                texturePathToIndex[normalizePath(embeddedName)] = newIdx;
                                                return newIdx;
                                            }
                                        } catch (...) {
                                            // ignore stoi errors
                                        }
                                    }

                // Если список скомпилированных текстур пуст — не спамим предупреждения (возможно этап подготовки текстур не выполнялся)
                if (!norm.empty() && !compiledTexturePaths.empty()) {
                    std::cerr << "[AssetProcessor WARNING] Texture not found in compiled list: " << norm << std::endl;
                }
            }
            return format::INVALID_INDEX_I32;  // Используем sentinel вместо жесткого -1
        };

        auto toLower = [](std::string s) {
            for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };

        auto findTextureByNameHints = [&](const std::vector<std::string>& includeHints,
                                          const std::vector<std::string>& excludeHints) -> int32_t {
            static constexpr std::array candidateTypes{
                aiTextureType_BASE_COLOR,
                aiTextureType_DIFFUSE,
                aiTextureType_UNKNOWN,
                aiTextureType_SPECULAR,
                aiTextureType_EMISSIVE,
                aiTextureType_NORMAL_CAMERA,
                aiTextureType_NORMALS,
                aiTextureType_HEIGHT
            };

            for (aiTextureType const t : candidateTypes) {
                const unsigned int count = aiMat->GetTextureCount(t);
                for (unsigned int ti = 0; ti < count; ++ti) {
                    aiString p;
                    if (aiMat->GetTexture(t, ti, &p) != AI_SUCCESS) continue;
                    std::string lname = toLower(filenameFromPath(normalizePath(std::string(p.C_Str()))));

                    bool hasInclude = includeHints.empty();
                    for (const auto& h : includeHints) {
                        if (lname.find(h) != std::string::npos) {
                            hasInclude = true;
                            break;
                        }
                    }
                    if (!hasInclude) continue;

                    bool hasExclude = false;
                    for (const auto& h : excludeHints) {
                        if (lname.find(h) != std::string::npos) {
                            hasExclude = true;
                            break;
                        }
                    }
                    if (hasExclude) continue;

                    int32_t idx = getTextureIndex(t, ti);
                    if (idx != format::INVALID_INDEX_I32) return idx;
                }
            }
            return format::INVALID_INDEX_I32;
        };

        auto getOrAddTexturePath = [&](const std::string& rawPath) -> int32_t {
            std::string norm = normalizePath(rawPath);
            auto it = texturePathToIndex.find(norm);
            if (it != texturePathToIndex.end()) return it->second;

            try {
                std::filesystem::path p(norm);
                if (!norm.empty() && std::filesystem::exists(p)) {
                    auto newIdx = static_cast<int32_t>(compiledTexturePaths.size());
                    compiledTexturePaths.push_back(norm);
                    texturePathToIndex[norm] = newIdx;
                    return newIdx;
                }

                if (!norm.empty()) {
                    std::filesystem::path resolved = sourceDir / p;
                    if (std::filesystem::exists(resolved)) {
                        std::string resolvedStr = normalizePath(resolved.lexically_normal().string());
                        auto found = texturePathToIndex.find(resolvedStr);
                        if (found != texturePathToIndex.end()) return found->second;

                        auto newIdx = static_cast<int32_t>(compiledTexturePaths.size());
                        compiledTexturePaths.push_back(resolvedStr);
                        texturePathToIndex[resolvedStr] = newIdx;
                        texturePathToIndex[norm] = newIdx;
                        return newIdx;
                    }
                }
            } catch (...) {
            }

            return format::INVALID_INDEX_I32;
        };

        auto inferAlbedoFromSibling = [&](int32_t refIdx) -> int32_t {
            if (refIdx == format::INVALID_INDEX_I32) return format::INVALID_INDEX_I32;
            if (refIdx < 0 || refIdx >= static_cast<int32_t>(compiledTexturePaths.size())) return format::INVALID_INDEX_I32;

            std::string p = compiledTexturePaths[refIdx];
            auto replaceToken = [&](const std::string& from, const std::string& to) -> std::string {
                std::string r = p;
                size_t pos = r.find(from);
                if (pos != std::string::npos) r.replace(pos, from.size(), to);
                return r;
            };

            std::vector<std::string> candidates{
                replaceToken("_Specular", "_BaseColor"),
                replaceToken("_specular", "_basecolor"),
                replaceToken("Specular", "BaseColor"),
                replaceToken("specular", "basecolor"),
                replaceToken("_spec", "_basecolor"),
                replaceToken("Spec", "BaseColor"),
                replaceToken("_Roughness", "_BaseColor"),
                replaceToken("_roughness", "_basecolor"),
                replaceToken("_Metallic", "_BaseColor"),
                replaceToken("_metallic", "_basecolor"),
                replaceToken("_Normal", "_BaseColor"),
                replaceToken("_normal", "_basecolor"),
                replaceToken("_orm", "_basecolor"),
                replaceToken("_ORM", "_BaseColor")
            };

            for (const auto& c : candidates) {
                int32_t idx = getOrAddTexturePath(c);
                if (idx != format::INVALID_INDEX_I32) return idx;
            }
            return format::INVALID_INDEX_I32;
        };

        auto albedoNameScore = [&](const std::string& lname) -> int {
            int score = 0;
            if (lname.find("basecolor") != std::string::npos || lname.find("base_color") != std::string::npos) score += 200;
            if (lname.find("albedo") != std::string::npos) score += 180;
            if (lname.find("diffuse") != std::string::npos) score += 120;
            if (lname.find("color") != std::string::npos) score += 40;

            if (lname.find("spec") != std::string::npos || lname.find("specular") != std::string::npos) score -= 220;
            if (lname.find("rough") != std::string::npos || lname.find("metal") != std::string::npos) score -= 180;
            if (lname.find("normal") != std::string::npos || lname.find("nrm") != std::string::npos) score -= 220;
            if (lname.find("ao") != std::string::npos || lname.find("occlusion") != std::string::npos) score -= 180;
            if (lname.find("height") != std::string::npos || lname.find("disp") != std::string::npos) score -= 160;
            if (lname.find("emissive") != std::string::npos || lname.find("emit") != std::string::npos) score -= 180;
            return score;
        };

        auto findBestAlbedoTexture = [&]() -> int32_t {
            static constexpr std::array candidateTypes{
                aiTextureType_BASE_COLOR,
                aiTextureType_DIFFUSE,
                aiTextureType_UNKNOWN,
                aiTextureType_SPECULAR,
                aiTextureType_EMISSIVE,
                aiTextureType_NORMAL_CAMERA,
                aiTextureType_NORMALS,
                aiTextureType_HEIGHT
            };

            int32_t bestIdx = format::INVALID_INDEX_I32;
            int bestScore = -1000000;
            for (aiTextureType t : candidateTypes) {
                const unsigned int count = aiMat->GetTextureCount(t);
                for (unsigned int ti = 0; ti < count; ++ti) {
                    aiString p;
                    if (aiMat->GetTexture(t, ti, &p) != AI_SUCCESS) continue;
                    std::string lname = toLower(filenameFromPath(normalizePath(std::string(p.C_Str()))));
                    int score = albedoNameScore(lname);
                    if (score <= 0) continue;
                    int32_t idx = getTextureIndex(t, ti);
                    if (idx == format::INVALID_INDEX_I32) continue;
                    if (score > bestScore) {
                        bestScore = score;
                        bestIdx = idx;
                    }
                }
            }
            return bestIdx;
        };

        auto findAlbedoInCatalog = [&]() -> int32_t {
            aiString matNameStr;
            std::string matNameLower;
            if (aiMat->Get(AI_MATKEY_NAME, matNameStr) == AI_SUCCESS) {
                matNameLower = toLowerStr(std::string(matNameStr.C_Str()));
            }

            auto collectTokens = [&](const std::string& s, std::vector<std::string>& out) {
                std::string cur;
                for (char ch : s) {
                    if (std::isalnum(static_cast<unsigned char>(ch))) cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                    else {
                        if (cur.size() >= 4) out.push_back(cur);
                        cur.clear();
                    }
                }
                if (cur.size() >= 4) out.push_back(cur);
            };

            std::vector<std::string> materialTokens;
            collectTokens(matNameLower, materialTokens);

            // Also use names of textures already referenced by this material (spec/normal/orm/emissive).
            std::array<aiTextureType, 8> refTypes{
                aiTextureType_SPECULAR, aiTextureType_UNKNOWN, aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS,
                aiTextureType_GLTF_METALLIC_ROUGHNESS, aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_EMISSIVE
            };
            for (aiTextureType t : refTypes) {
                const unsigned int count = aiMat->GetTextureCount(t);
                for (unsigned int ti = 0; ti < count; ++ti) {
                    aiString p;
                    if (aiMat->GetTexture(t, ti, &p) != AI_SUCCESS) continue;
                    std::string name = toLowerStr(filenameFromPath(normalizePath(std::string(p.C_Str()))));
                    collectTokens(name, materialTokens);
                }
            }

            int bestScore = -1000000;
            std::string bestPath;
            for (const auto& item : textureCatalog) {
                const std::string& n = item.lowerName;
                bool albedoLike =
                    n.find("basecolor") != std::string::npos ||
                    n.find("base_color") != std::string::npos ||
                    n.find("albedo") != std::string::npos ||
                    n.find("diffuse") != std::string::npos;
                if (!albedoLike) continue;

                bool bad =
                    n.find("spec") != std::string::npos ||
                    n.find("rough") != std::string::npos ||
                    n.find("metal") != std::string::npos ||
                    n.find("normal") != std::string::npos ||
                    n.find("nrm") != std::string::npos ||
                    n.find("ao") != std::string::npos ||
                    n.find("height") != std::string::npos ||
                    n.find("emissive") != std::string::npos;
                if (bad) continue;

                int score = 0;
                if (n.find("basecolor") != std::string::npos || n.find("base_color") != std::string::npos) score += 200;
                if (n.find("albedo") != std::string::npos) score += 160;
                if (n.find("diffuse") != std::string::npos) score += 120;

                int matchedTokens = 0;
                for (const auto& t : materialTokens) {
                    if (n.find(t) != std::string::npos) {
                        score += 30;
                        matchedTokens++;
                    }
                }
                if (matchedTokens == 0) score -= 100;

                if (score > bestScore) {
                    bestScore = score;
                    bestPath = item.path;
                }
            }

            if (bestPath.empty()) return format::INVALID_INDEX_I32;
            return getOrAddTexturePath(bestPath);
        };

        // Old mixed heuristics removed — texture indices are resolved below by the new Assimp-first resolver.

        // 2b. Полная переоценка индексов текстур:
        // сначала строго по ключам Assimp, затем fallback по именам файлов.
        auto pickByAssimpTypes = [&](const std::vector<aiTextureType>& types) -> int32_t {
            for (aiTextureType t : types) {
                const unsigned int count = aiMat->GetTextureCount(t);
                for (unsigned int s = 0; s < count; ++s) {
                    int32_t idx = getTextureIndex(t, s);
                    if (idx != format::INVALID_INDEX_I32) return idx;
                }
            }
            return format::INVALID_INDEX_I32;
        };

        auto collectTokens = [&](const std::string& str, std::vector<std::string>& out) {
            std::string cur;
            for (char ch : str) {
                if (std::isalnum(static_cast<unsigned char>(ch))) {
                    cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                } else {
                    if (cur.size() >= 4) out.push_back(cur);
                    cur.clear();
                }
            }
            if (cur.size() >= 4) out.push_back(cur);
        };

        std::vector<std::string> matTokens;
        std::string materialName = "UnnamedMaterial";
        {
            aiString matNameStr;
            if (aiMat->Get(AI_MATKEY_NAME, matNameStr) == AI_SUCCESS) {
                materialName = matNameStr.C_Str();
                collectTokens(materialName, matTokens);
            }
        }

        auto pickFromCatalog = [&](const std::vector<std::string>& include, const std::vector<std::string>& exclude) -> int32_t {
            int bestScore = -1000000;
            std::string bestPath;
            for (const auto& item : textureCatalog) {
                const std::string& n = item.lowerName;

                bool hasInclude = false;
                for (const auto& k : include) {
                    if (n.find(k) != std::string::npos) { hasInclude = true; break; }
                }
                if (!hasInclude) continue;

                bool hasExclude = false;
                for (const auto& k : exclude) {
                    if (n.find(k) != std::string::npos) { hasExclude = true; break; }
                }
                if (hasExclude) continue;

                int score = 0;
                for (const auto& t : matTokens) {
                    if (n.find(t) != std::string::npos) score += 25;
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestPath = item.path;
                }
            }
            if (bestPath.empty()) return format::INVALID_INDEX_I32;
            return getOrAddTexturePath(bestPath);
        };

        int32_t resolvedAlbedo = pickByAssimpTypes({aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE});
        const char* albedoSource = "assimp";
        int32_t resolvedNormal = pickByAssimpTypes({aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS});
        int32_t resolvedOrm = pickByAssimpTypes({
            aiTextureType_GLTF_METALLIC_ROUGHNESS,
            aiTextureType_METALNESS,
            aiTextureType_DIFFUSE_ROUGHNESS,
            aiTextureType_AMBIENT,
            aiTextureType_SPECULAR,
            aiTextureType_UNKNOWN
        });
        int32_t resolvedEmissive = pickByAssimpTypes({aiTextureType_EMISSIVE});
        int32_t resolvedHeight = pickByAssimpTypes({aiTextureType_HEIGHT});

        if (resolvedAlbedo == format::INVALID_INDEX_I32) {
            int32_t inferred = inferAlbedoFromSibling(resolvedOrm);
            if (inferred == format::INVALID_INDEX_I32) inferred = inferAlbedoFromSibling(resolvedNormal);
            if (inferred == format::INVALID_INDEX_I32) inferred = inferAlbedoFromSibling(resolvedEmissive);
            if (inferred != format::INVALID_INDEX_I32) {
                resolvedAlbedo = inferred;
                albedoSource = "sibling";
            }
        }

        if (resolvedAlbedo == format::INVALID_INDEX_I32) {
            resolvedAlbedo = pickFromCatalog(
                {"basecolor", "base_color", "albedo", "diffuse", "color"},
                {"specular", "_spec", "roughness", "metallic", "metalness", "normal", "nrm", "_ao", "occlusion", "height", "emissive"}
            );
            if (resolvedAlbedo != format::INVALID_INDEX_I32) {
                albedoSource = "catalog";
            }
        }
        if (resolvedNormal == format::INVALID_INDEX_I32) {
            resolvedNormal = pickFromCatalog(
                {"normal", "nrm", "nor"},
                {"basecolor", "base_color", "albedo", "diffuse", "specular", "roughness", "metallic", "metalness", "emissive"}
            );
        }
        if (resolvedOrm == format::INVALID_INDEX_I32) {
            resolvedOrm = pickFromCatalog(
                {"orm", "specular", "roughness", "metallic", "metalness", "arm", "rma", "mra", "occlusion", "_ao"},
                {"basecolor", "base_color", "albedo", "diffuse", "normal", "nrm", "emissive"}
            );
        }
        // Emissive/height are intentionally strict: do not auto-pick from catalog when Assimp keys are absent.
        // Otherwise unrelated glow/height maps get injected into most materials and tint the whole scene.

        myMat.albedoTexIdx   = resolvedAlbedo;
        myMat.normalTexIdx   = resolvedNormal;
        myMat.ormTexIdx      = resolvedOrm;
        myMat.emissiveTexIdx = resolvedEmissive;
        myMat.heightTexIdx   = resolvedHeight;

        if (metalicFactorReturn == format::INVALID_INDEX_I32) {
            if (resolvedOrm == format::INVALID_INDEX_I32) {
                myMat.metallicFactor = 0.0f;
            }
            else {
                myMat.metallicFactor = 1.0f;
            }
        }

        // If neither emissive color nor emissive texture is provided, force emissive off.
        if (resolvedEmissive == format::INVALID_INDEX_I32 &&
            myMat.emissiveFactor.r == 0.0f &&
            myMat.emissiveFactor.g == 0.0f &&
            myMat.emissiveFactor.b == 0.0f) {
            myMat.emissiveStrength = 0.0f;
        }

        auto texName = [&](int32_t idx) -> std::string {
            if (idx == format::INVALID_INDEX_I32 || idx < 0 || idx >= static_cast<int32_t>(compiledTexturePaths.size())) {
                return "<none>";
            }
            return filenameFromPath(normalizePath(compiledTexturePaths[idx]));
        };
        std::cout << "[AssetProcessor MAT] #" << i << " " << materialName
                  << " | albedo=" << myMat.albedoTexIdx << " [" << albedoSource << "] " << texName(myMat.albedoTexIdx)
                  << " | normal=" << myMat.normalTexIdx << " " << texName(myMat.normalTexIdx)
                  << " | orm=" << myMat.ormTexIdx << " " << texName(myMat.ormTexIdx)
                  << " | emissive=" << myMat.emissiveTexIdx << " " << texName(myMat.emissiveTexIdx)
                  << std::endl;

        // 3. Автоматическое вычисление флагов графического конвейера (Pipeline Flags)
        auto flags = static_cast<uint32_t>(format::MaterialPipelineFlagBits::eNone);

        // Определяем режим прозрачности альфа-канала
        aiString alphaMode;
        bool hasAlphaMode = (aiMat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS);
        if (hasAlphaMode) {
            std::string mode(alphaMode.C_Str());
            if (mode == "MASK") {
                flags |= static_cast<uint32_t>(format::MaterialPipelineFlagBits::eAlphaTest);
            } else if (mode == "BLEND") {
                flags |= static_cast<uint32_t>(format::MaterialPipelineFlagBits::eTransparent);
            }
        } else {
            // Fallback: если alpha базового цвета < alphaCutoff, включаем AlphaTest
            if (baseColor.a < alphaCutoff && baseColor.a < 1.0f) {
                flags |= static_cast<uint32_t>(format::MaterialPipelineFlagBits::eAlphaTest);
            }
        }

        // Определяем двусторонность материала (критично для листьев и флагов в Bistro)
        bool doubleSided = false;
        if (aiMat->Get(AI_MATKEY_TWOSIDED, doubleSided) == AI_SUCCESS && doubleSided) {
            flags |= static_cast<uint32_t>(format::MaterialPipelineFlagBits::eDoubleSided);
        }

        myMat.pipelineFlags = flags;
        myMat.padding = 0; // Зануляем заполнитель выравнивания

        outMaterials.push_back(myMat);
    }

    std::cout << "[AssetProcessor] Модуль материалов успешно скомпилирован. Всего материалов: "
              << outMaterials.size() << std::endl;
}

void SceneImporter::parseLights(
    const aiScene* scene,
    std::vector<format::DirectionalLight>& outDirLights,
    std::vector<format::PointLight>& outPointLights,
    std::vector<format::SpotLight>& outSpotLights
) {
    if (!scene || scene->mNumLights == 0) return;

    // Предварительно резервируем место под источники света
    outDirLights.reserve(2);
    outPointLights.reserve(scene->mNumLights);
    outSpotLights.reserve(scene->mNumLights);

    for (uint32_t i = 0; i < scene->mNumLights; ++i) {
        aiLight* aiLt = scene->mLights[i];
        // Переводим цвета и интенсивность из Assimp в единый вектор цвета
        glm::vec3 color(aiLt->mColorDiffuse.r, aiLt->mColorDiffuse.g, aiLt->mColorDiffuse.b);

        // Позиция и направление источника света по умолчанию
        glm::vec3 pos(aiLt->mPosition.x, aiLt->mPosition.y, aiLt->mPosition.z);
        glm::vec3 dir(aiLt->mDirection.x, aiLt->mDirection.y, aiLt->mDirection.z);

        switch (aiLt->mType) {
            // =========================================================================
            // 1. НАПРАВЛЕННЫЙ СВЕТ (Солнце / Наше будущее CSM) — 32 байта [т-ж]
            // =========================================================================
            case aiLightSource_DIRECTIONAL: {
                format::DirectionalLight dLt{};
                dLt.directionAndIntensity = glm::vec4(glm::normalize(dir), 1.0); // Интенсивность зашиваем в W
                dLt.color = glm::vec4(color, 1.0f);
                outDirLights.push_back(dLt);
                break;
            }

            // =========================================================================
            // 2. ТОЧЕЧНЫЙ СВЕТ (Лампочки на веранде Bistro) — 32 байта [т-ж]
            // =========================================================================
            case aiLightSource_POINT: {
                format::PointLight pLt{};
                // Вычисляем радиус затухания на основе коэффициентов аттенюации Assimp
                // Формула падения света: 1.0 / (constant + linear*d + quadratic*d^2)
                // Для MVP берем радиус условно по падению интенсивности, либо закладываем радиус в mAttenuationLinear
                float radius = (aiLt->mAttenuationLinear > 0.0f) ? (1.0f / aiLt->mAttenuationLinear) : 10.0f;

                pLt.positionAndRadius = glm::vec4(pos, radius); // Радиус запекаем в компоненту W [т-ж]
                pLt.color = glm::vec4(color, 1.0f);
                outPointLights.push_back(pLt);
                break;
            }

            // =========================================================================
            // 3. ПРОЖЕКТОРНЫЙ СВЕТ (Фонари направленного действия) — 64 байта [т-ж]
            // =========================================================================
            case aiLightSource_SPOT: {
                format::SpotLight sLt{};
                float radius = (aiLt->mAttenuationLinear > 0.0f) ? (1.0f / aiLt->mAttenuationLinear) : 15.0f;

                sLt.positionAndRadius = glm::vec4(pos, radius);
                sLt.directionAndIntensity = glm::vec4(glm::normalize(dir), 1.0f);
                sLt.color = glm::vec4(color, 1.0f);

                // Косинусы углов отсечения внутреннего и внешнего конуса для плавного сглаживания краев пятна в шейдере
                sLt.innerCutOffCos = std::cos(aiLt->mAngleInnerCone);
                sLt.outerCutOffCos = std::cos(aiLt->mAngleOuterCone);

                outSpotLights.push_back(sLt);
                break;
            }

            default:
                break;
        }
    }

    std::cout << "[AssetProcessor] Модуль источников света успешно скомпилирован!" << std::endl;
    std::cout << "  -> Направленных (Солнц): " << outDirLights.size() << std::endl;
    std::cout << "  -> Точечных (Лампочек): " << outPointLights.size() << std::endl;
    std::cout << "  -> Прожекторов:         " << outSpotLights.size() << std::endl;
}

void SceneImporter::processGeometry(
    const aiScene* scene,
    std::vector<format::MeshHeader>& outMeshes,
    std::vector<uint8_t>& globalBulkData
) {
    if (!scene || scene->mNumMeshes == 0) return;

    const uint32_t meshCount = scene->mNumMeshes;

    // Переменные для результатов по-каждому мешу (чтобы безопасно параллелить)
    std::vector<format::MeshHeader> perMeshHeaders(meshCount);
    std::vector<std::vector<uint8_t>> perMeshPositionBytes(meshCount);
    std::vector<std::vector<uint8_t>> perMeshAttributeBytes(meshCount);
    std::vector<std::vector<uint32_t>> perMeshIndices(meshCount);
    std::vector<uint32_t> perMeshLodCounts(meshCount, 0);

    omp_set_num_threads(omp_get_max_threads());

    #pragma omp parallel for default(none) shared(scene, meshCount, perMeshHeaders, perMeshPositionBytes, perMeshAttributeBytes, perMeshIndices, perMeshLodCounts)
    for (int meshIdx = 0; meshIdx < static_cast<int>(meshCount); ++meshIdx) {
        aiMesh* aiMesh = scene->mMeshes[meshIdx];
        if (!aiMesh || aiMesh->mNumVertices == 0) continue;

        // 1. Извлекаем сырые данные из Assimp
        struct PackedVertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec4 tangent;
        };

        std::vector<PackedVertex> packedVertices(aiMesh->mNumVertices);
        std::vector<uint32_t> rawIndices;
        rawIndices.reserve(aiMesh->mNumFaces * 3);

        for (uint32_t v = 0; v < aiMesh->mNumVertices; ++v) {
            PackedVertex pv{};
            pv.pos = glm::vec3(aiMesh->mVertices[v].x, aiMesh->mVertices[v].y, aiMesh->mVertices[v].z);
            pv.normal = aiMesh->HasNormals() ? glm::vec3(aiMesh->mNormals[v].x, aiMesh->mNormals[v].y, aiMesh->mNormals[v].z) : glm::vec3(0.0f);
            pv.uv = aiMesh->HasTextureCoords(0) ? glm::vec2(aiMesh->mTextureCoords[0][v].x, aiMesh->mTextureCoords[0][v].y) : glm::vec2(0.0f);
            pv.tangent = aiMesh->HasTangentsAndBitangents() ? glm::vec4(aiMesh->mTangents[v].x, aiMesh->mTangents[v].y, aiMesh->mTangents[v].z, 1.0f) : glm::vec4(0.0f);
            packedVertices[v] = pv;
        }

        for (uint32_t f = 0; f < aiMesh->mNumFaces; ++f) {
            const aiFace& face = aiMesh->mFaces[f];
            if (face.mNumIndices == 3) {
                rawIndices.push_back(face.mIndices[0]);
                rawIndices.push_back(face.mIndices[1]);
                rawIndices.push_back(face.mIndices[2]);
            }
        }

        // Ремап
        std::vector<unsigned int> remap(aiMesh->mNumVertices);
        size_t totalUniqueVertices = meshopt_generateVertexRemap(
            remap.data(), rawIndices.data(), rawIndices.size(),
            packedVertices.data(), aiMesh->mNumVertices, sizeof(PackedVertex)
        );

        std::vector<PackedVertex> uniquePacked(totalUniqueVertices);
        std::vector<uint32_t> uniqueIndices(rawIndices.size());

        meshopt_remapVertexBuffer(uniquePacked.data(), packedVertices.data(), aiMesh->mNumVertices, sizeof(PackedVertex), remap.data());
        meshopt_remapIndexBuffer(uniqueIndices.data(), rawIndices.data(), rawIndices.size(), remap.data());

        // Собираем myMesh
        format::MeshHeader myMesh{};
        myMesh.defaultMaterialIndex = static_cast<int32_t>(aiMesh->mMaterialIndex);

        // Генерируем LOD'ы (локальные индексы) — логика как раньше
        std::vector<uint32_t> currentLodIndices = uniqueIndices;
        uint32_t actualLodCount = 0;
        for (int lod = 0; lod < 4; ++lod) {
            float screenThresholds[4] = { 0.5f, 0.2f, 0.08f, 0.01f };
            if (lod > 0) {
                float lodThresholds[4] = { 1.0f, 0.60f, 0.30f, 0.10f };
                auto targetIndexCount = static_cast<size_t>(static_cast<float>(currentLodIndices.size()) * lodThresholds[lod]);
                if (targetIndexCount < 3) {
                    // keep
                } else {
                    float targetError = 1e-2f;
                    std::vector<uint32_t> simplifiedIndices(currentLodIndices.size());
                    size_t resultSize = meshopt_simplify(
                        simplifiedIndices.data(), currentLodIndices.data(), currentLodIndices.size(),
                        reinterpret_cast<const float*>(uniquePacked.data()), uniquePacked.size(),
                        sizeof(PackedVertex), targetIndexCount, targetError
                    );
                    simplifiedIndices.resize(resultSize);
                    if (resultSize == currentLodIndices.size() && lod > 1) {
                        break;
                    }
                    currentLodIndices = simplifiedIndices;
                }
            }

            // Only reorder vertex arrays for LOD 0. LOD 1+ must not touch the vertex arrays
            // because the already-stored LOD 0 indices would become invalid.
            if (lod == 0) {
                std::vector<unsigned int> fetchRemap(uniquePacked.size());
                meshopt_optimizeVertexFetchRemap(
                    fetchRemap.data(),
                    currentLodIndices.data(),
                    currentLodIndices.size(),
                    uniquePacked.size()
                );
            }

            format::MeshLODData& lodData = myMesh.lods[lod];
            lodData.indexCount = static_cast<uint32_t>(currentLodIndices.size());
            lodData.firstIndex = 0; // will be fixed during merge
            lodData.vertexOffset = 0; // will be fixed
            lodData.materialIndexOverride = -1;
            lodData.lodScreenSizeThreshold = screenThresholds[lod];

            glm::vec3 minB(1e10f), maxB(-1e10f);
            for (const auto& idx : currentLodIndices) {
                const glm::vec3& pos = uniquePacked[idx].pos;
                minB = glm::min(minB, pos);
                maxB = glm::max(maxB, pos);
            }
            lodData.minBounds = minB;
            lodData.maxBounds = maxB;

            // Сохраняем индексы этого ЛОДа в perMeshIndices
            perMeshIndices[meshIdx].insert(perMeshIndices[meshIdx].end(), currentLodIndices.begin(), currentLodIndices.end());
            actualLodCount++;
        }

        // Разделяем обратно на позиции и атрибуты
        std::vector<glm::vec3> uniquePositions(totalUniqueVertices);
        struct ExtraAttributes { glm::vec3 normal; glm::vec2 uv; glm::vec4 tangent; };
        std::vector<ExtraAttributes> uniqueAttributes(totalUniqueVertices);
        for (size_t i = 0; i < totalUniqueVertices; ++i) {
            uniquePositions[i] = uniquePacked[i].pos;
            uniqueAttributes[i].normal = uniquePacked[i].normal;
            uniqueAttributes[i].uv = uniquePacked[i].uv;
            uniqueAttributes[i].tangent = uniquePacked[i].tangent;
        }

        uint32_t attrFlags = 0;
        if (aiMesh->HasNormals()) attrFlags |= static_cast<uint32_t>(format::MeshAttributeFlags::eHasNormal);
        if (aiMesh->HasTextureCoords(0)) attrFlags |= static_cast<uint32_t>(format::MeshAttributeFlags::eHasUV);
        if (aiMesh->HasTangentsAndBitangents()) attrFlags |= static_cast<uint32_t>(format::MeshAttributeFlags::eHasTangent);
        myMesh.attributeFlags = attrFlags;

        myMesh.lodCount = actualLodCount;
        perMeshHeaders[meshIdx] = myMesh;

        // Позиции и атрибуты
        perMeshPositionBytes[meshIdx].resize(uniquePositions.size() * sizeof(glm::vec3));
        std::memcpy(perMeshPositionBytes[meshIdx].data(), uniquePositions.data(), uniquePositions.size() * sizeof(glm::vec3));

        perMeshAttributeBytes[meshIdx].resize(uniqueAttributes.size() * sizeof(ExtraAttributes));
        std::memcpy(perMeshAttributeBytes[meshIdx].data(), uniqueAttributes.data(), uniqueAttributes.size() * sizeof(ExtraAttributes));

        perMeshLodCounts[meshIdx] = actualLodCount;
    }

    // Теперь объединяем результаты в порядке meshIdx
    std::vector<uint8_t> totalPositionBytes;
    std::vector<uint8_t> totalAttributeBytes;
    std::vector<uint32_t> totalIndices;

    totalPositionBytes.reserve(1024);
    totalAttributeBytes.reserve(1024);
    totalIndices.reserve(1024);

    for (uint32_t meshIdx = 0; meshIdx < meshCount; ++meshIdx) {
        const auto &myMesh = perMeshHeaders[meshIdx];
        if (perMeshPositionBytes[meshIdx].empty() && perMeshIndices[meshIdx].empty()) continue;

        format::MeshHeader meshOut = myMesh; // copy

        // offsets
        auto baseVertexOffset = static_cast<int32_t>(totalPositionBytes.size() / sizeof(glm::vec3));
        meshOut.positionBufferAddress = totalPositionBytes.size();
        meshOut.attributeBufferAddress = totalAttributeBytes.size();
        meshOut.indexBufferAddress = totalIndices.size() * sizeof(uint32_t);

        // Fix LOD firstIndex/vertexOffset
        auto idxCursor = static_cast<uint32_t>(totalIndices.size());
        for (int lod = 0; lod < static_cast<int>(meshOut.lodCount); ++lod) {
            meshOut.lods[lod].firstIndex = idxCursor;
            meshOut.lods[lod].vertexOffset = baseVertexOffset;
            idxCursor += meshOut.lods[lod].indexCount;
        }

        // Append indices
        if (!perMeshIndices[meshIdx].empty()) {
            totalIndices.insert(totalIndices.end(), perMeshIndices[meshIdx].begin(), perMeshIndices[meshIdx].end());
        }

        // Append vertex data
        if (!perMeshPositionBytes[meshIdx].empty()) {
            totalPositionBytes.insert(totalPositionBytes.end(), perMeshPositionBytes[meshIdx].begin(), perMeshPositionBytes[meshIdx].end());
        }
        if (!perMeshAttributeBytes[meshIdx].empty()) {
            totalAttributeBytes.insert(totalAttributeBytes.end(), perMeshAttributeBytes[meshIdx].begin(), perMeshAttributeBytes[meshIdx].end());
        }

        outMeshes.push_back(meshOut);
    }

    // =========================================================================
    // ШАГ 5: СЛИЯНИЕ ВСЕХ БУФЕРОВ В МОНОЛИТНЫЙ BULK DATA BLOB ДЛЯ ДИСКА
    // =========================================================================
    auto alignSize = [](size_t size) { return (size + 15) & ~15; };

    size_t sizePos  = alignSize(totalPositionBytes.size());
    size_t sizeAttr = alignSize(totalAttributeBytes.size());
    size_t sizeIdx  = alignSize(totalIndices.size() * sizeof(uint32_t));

    globalBulkData.resize(sizePos + sizeAttr + sizeIdx);

    // Копируем позиции
    if (!totalPositionBytes.empty()) std::memcpy(globalBulkData.data(), totalPositionBytes.data(), totalPositionBytes.size());
    // Копируем атрибуты следом
    if (!totalAttributeBytes.empty()) std::memcpy(globalBulkData.data() + sizePos, totalAttributeBytes.data(), totalAttributeBytes.size());
    // Копируем индексы в самый хвост
    if (!totalIndices.empty()) std::memcpy(globalBulkData.data() + sizePos + sizeAttr, totalIndices.data(), totalIndices.size() * sizeof(uint32_t));
    // Обновляем оффсеты в заголовках мешей, чтобы они указывали на точное смещение ВНУТРИ глобального bulkDataOffset!
    for (auto& mesh : outMeshes) {
        mesh.positionBufferAddress  += 0;
        mesh.attributeBufferAddress += sizePos;
        mesh.indexBufferAddress     += (sizePos + sizeAttr);
    }
    std::cout << "[AssetProcessor] Геометрия успешно запечена в блоб через meshoptimizer!" << std::endl;
    std::cout << "  -> Всего мeshей упаковано: " << outMeshes.size() << std::endl;
    std::cout << "  -> Общий вес сырой геометрии: " << (static_cast<double>(globalBulkData.size()) / (1024.0 * 1024.0)) << " МБ." << std::endl;
}

// Вспомогательная функция для конвертации матриц Assimp в GLM
static inline glm::mat4 convertMatrix4x4(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

void SceneImporter::parseAnimations(
    const aiScene* scene,
    const std::vector<format::SceneNode>& compiledNodes,
    std::vector<format::SkeletonData>& outSkeletons,
    std::vector<format::BoneData>& outBones,
    std::vector<format::AnimationClip>& outClips,
    std::vector<format::BoneChannel>& outBoneChannels,
    std::vector<format::MorphTarget>& outMorphTargets,       // УЧТЕНО!
    std::vector<format::MorphChannel>& outMorphChannels,     // УЧТЕНО!
    std::vector<format::MaterialProperty>& outMatProperties, // УЧТЕНО!
    std::vector<format::MaterialChannel>& outMatChannels,     // УЧТЕНО!
    std::vector<float>& globalKeyframeTimes,
    std::vector<format::AnimationKeyframeValue>& globalKeyframeValues
) {
    if (!scene) return;

        // Хэш-мапа для быстрого сопоставления хешей имён нод с их индексами в плоском графе
        std::unordered_map<uint32_t, uint32_t> nodeNameHashToIndex;
        for (size_t i = 0; i < compiledNodes.size(); ++i) {
            uint32_t nodeHash = compiledNodes[i].nodeNameHash;
            if (nodeHash != format::INVALID_HASH) {
                nodeNameHashToIndex[nodeHash] = static_cast<uint32_t>(i);
            }
        }

    // =========================================================================
    // ЭТАП 1: СКЕЛЕТНАЯ АНИМАЦИЯ И СКИННИНГ (BONE SKINNING)
    // =========================================================================
    if (scene->mNumMeshes > 0) {
        format::SkeletonData mainSkeleton{};
        mainSkeleton.boneOffset = static_cast<uint32_t>(outBones.size());
        mainSkeleton.rootBoneIndex = format::INVALID_INDEX_I32; // По умолчанию корня нет

        // Соберем все кости локально, затем построим обратную маппинг nodeIndex->boneIndex и заполним parentIndex корректно
        std::vector<format::BoneData> localBones;
        localBones.reserve(128);

        for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
            aiMesh* mesh = scene->mMeshes[m];
            if (!mesh->HasBones()) continue;

            for (uint32_t b = 0; b < mesh->mNumBones; ++b) {
                aiBone* bone = mesh->mBones[b];
                std::string boneName(bone->mName.C_Str());

                format::BoneData boneData{};
                boneData.invBindMatrix = convertMatrix4x4(bone->mOffsetMatrix);
                boneData.parentIndex = format::INVALID_INDEX_I32;
                boneData.nodeIndex = format::INVALID_INDEX_U32;

                // Сопоставляем кость с нодой через хеш имени
                uint32_t boneNameHash = fnv1a_hash(boneName);
                auto it = nodeNameHashToIndex.find(boneNameHash);
                if (it != nodeNameHashToIndex.end()) {
                    boneData.nodeIndex = it->second;
                }
                localBones.push_back(boneData);
            }
        }

        // Построим обратную маппинг: nodeIndex -> boneIndex (внутри скелета)
        std::unordered_map<uint32_t, uint32_t> nodeIndexToBoneIndex;
        for (uint32_t bi = 0; bi < localBones.size(); ++bi) {
            uint32_t nidx = localBones[bi].nodeIndex;
            if (nidx != format::INVALID_INDEX_U32) nodeIndexToBoneIndex[nidx] = bi;
        }

        // Второй проход — назначаем parentIndex для каждой кости на основе parent ноды
        for (auto & localBone : localBones) {
            uint32_t nidx = localBone.nodeIndex;
            if (nidx != format::INVALID_INDEX_U32 && nidx < compiledNodes.size()) {
                uint32_t parentNode = compiledNodes[nidx].parentIndex;
                if (parentNode != format::INVALID_INDEX_U32) {
                    auto pit = nodeIndexToBoneIndex.find(parentNode);
                    if (pit != nodeIndexToBoneIndex.end()) {
                        localBone.parentIndex = static_cast<int32_t>(pit->second);
                    } else {
                        localBone.parentIndex = format::INVALID_INDEX_I32;
                    }
                } else {
                    localBone.parentIndex = format::INVALID_INDEX_I32;
                }
            } else {
                localBone.parentIndex = format::INVALID_INDEX_I32;
            }
        }

        // Определим индекс корня скелета (первая кость без родителя)
        int32_t rootLocalIdx = format::INVALID_INDEX_I32;
        for (uint32_t bi = 0; bi < localBones.size(); ++bi) {
            if (localBones[bi].parentIndex == format::INVALID_INDEX_I32) { rootLocalIdx = static_cast<int32_t>(bi); break; }
        }

        if (!localBones.empty()) {
            mainSkeleton.boneCount = static_cast<uint32_t>(localBones.size());
            mainSkeleton.explicitPadding = 0;
            mainSkeleton.rootBoneIndex = (rootLocalIdx == format::INVALID_INDEX_I32) ? format::INVALID_INDEX_I32 : rootLocalIdx;

            // Вписываем кости в глобальный массив
            for (auto &b : localBones) outBones.push_back(b);
            outSkeletons.push_back(mainSkeleton);
        }
    }

    // =========================================================================
    // ЭТАП 2: ГЕНЕРАЦИЯ ДАННЫХ МОРФИНГА (MORPH TARGETS / BLEND SHAPES)
    // =========================================================================
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];
        if (mesh->mNumAnimMeshes == 0) continue;

        // Каждый AnimMesh в Assimp — это отдельный бленд-шейп (цель морфинга)
        for (uint32_t am = 0; am < mesh->mNumAnimMeshes; ++am) {
            // aiAnimMesh* animMesh = mesh->mAnimMeshes[am]; // Не используется напрямую сейчас

            format::MorphTarget target{};
            target.firstDeltaGlobalIdx = 0;
            target.deltaCount = 0;
            target.targetNameHash = static_cast<uint32_t>(m);
            target.explicitPadding = 0;
            target.maxPositionDelta = glm::vec3(0.0f);
            target.explicitPadding2 = 0;

            outMorphTargets.push_back(target);
        }
    }

    // =========================================================================
    // ЭТАП 3: ИНИЦИАЛИЗАЦИЯ ТАБЛИЦ ДИНАМИЧЕСКИХ МАТЕРИАЛОВ
    // =========================================================================
    for (uint32_t matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx) {
        aiMaterial* mat = scene->mMaterials[matIdx];

        // Сканируем свойства, которые могут изменяться в рантайме (например, Emissive Intensity)
        aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
            format::MaterialProperty prop{};
            prop.propertyHash = 0; // TODO: заменить на реальный хэш имени свойства
            prop.elementCount = 4; // emissiveFactor — vec4
            prop.offsetInMaterial = static_cast<uint32_t>(offsetof(format::MaterialInfo, emissiveFactor));
            prop.explicitPadding = 0;

            outMatProperties.push_back(prop);
        }
    }

    // =========================================================================
    // ЭТАП 4: ПАРСИНГ АНИМАЦИОННЫХ КЛИПОВ, КАНАЛОВ И КЛЮЧЕВЫХ КАДРОВ
    // =========================================================================
    outClips.reserve(scene->mNumAnimations);

    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        aiAnimation* anim = scene->mAnimations[i];

        format::AnimationClip clip{};
        // Защита от деления на ноль
        float ticksPerSecond = (anim->mTicksPerSecond > 0.0) ? static_cast<float>(anim->mTicksPerSecond) : 1.0f;
        clip.duration = static_cast<float>(static_cast<double>(anim->mDuration) / static_cast<double>(ticksPerSecond));
        clip.clipNameHash = fnv1a_hash(anim->mName.C_Str());

        // Запоминаем стартовые точки каналов текущего клипа в глобальных таблицах
        clip.firstBoneChannelIdx  = static_cast<uint32_t>(outBoneChannels.size());
        clip.firstMorphChannelIdx = static_cast<uint32_t>(outMorphChannels.size());
        clip.firstMatChannelIdx   = static_cast<uint32_t>(outMatProperties.size()); // Смещение свойств материалов

        uint32_t activeBoneChannels = 0;
        uint32_t activeMorphChannels = 0;

        // 1. Парсим стандартные каналы трансформации костей/нод
        for (uint32_t j = 0; j < anim->mNumChannels; ++j) {
            aiNodeAnim* channel = anim->mChannels[j];
            std::string targetName(channel->mNodeName.C_Str());
            uint32_t targetNameHash = fnv1a_hash(targetName);
            auto it = nodeNameHashToIndex.find(targetNameHash);
            uint32_t currentBoneIdx = (it != nodeNameHashToIndex.end()) ? it->second : format::INVALID_INDEX_U32;

            // Канал Позиции (Translation)
            if (channel->mNumPositionKeys > 0) {
                format::BoneChannel bChan{};
                bChan.boneIndex = currentBoneIdx;
                bChan.pathType = 0; // Translation
                bChan.keyframeOffset = static_cast<uint32_t>(globalKeyframeTimes.size());
                bChan.keyframeCount = channel->mNumPositionKeys;

                for (uint32_t k = 0; k < channel->mNumPositionKeys; ++k) {
                    aiVectorKey key = channel->mPositionKeys[k];
                    globalKeyframeTimes.push_back(static_cast<float>(key.mTime / ticksPerSecond));
                    globalKeyframeValues.push_back({ glm::vec4(key.mValue.x, key.mValue.y, key.mValue.z, 1.0f) });
                }
                outBoneChannels.push_back(bChan);
                activeBoneChannels++;
            }

            // Канал Вращения (Rotation)
            if (channel->mNumRotationKeys > 0) {
                format::BoneChannel bChan{};
                bChan.boneIndex = currentBoneIdx;
                bChan.pathType = 1; // Rotation (Кватернион)
                bChan.keyframeOffset = static_cast<uint32_t>(globalKeyframeTimes.size());
                bChan.keyframeCount = channel->mNumRotationKeys;

                for (uint32_t k = 0; k < channel->mNumRotationKeys; ++k) {
                    aiQuatKey key = channel->mRotationKeys[k];
                    globalKeyframeTimes.push_back(static_cast<float>(key.mTime / ticksPerSecond));
                    globalKeyframeValues.push_back({ glm::vec4(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w) });
                }
                outBoneChannels.push_back(bChan);
                activeBoneChannels++;
            }
        }

        // 2. Парсим анимационные каналы весов морфинга (Blend Shape Animations)
        for (uint32_t j = 0; j < anim->mNumMeshChannels; ++j) {
            aiMeshAnim* mChannel = anim->mMeshChannels[j];

            format::MorphChannel morphChan{};
            morphChan.morphTargetId = j; // Соответствие целевому индексу деформации
            morphChan.keyframeOffset = static_cast<uint32_t>(globalKeyframeTimes.size());
            morphChan.keyframeCount = mChannel->mNumKeys;

            for (uint32_t k = 0; k < mChannel->mNumKeys; ++k) {
                aiMeshKey key = mChannel->mKeys[k];
                globalKeyframeTimes.push_back(static_cast<float>(key.mTime / ticksPerSecond));

                // Вес морфа записываем в компоненту X вектора vec4
                globalKeyframeValues.push_back({ glm::vec4(static_cast<float>(key.mValue), 0.0f, 0.0f, 0.0f) });
            }
            outMorphChannels.push_back(morphChan);
            activeMorphChannels++;
        }

        clip.boneChannelCount = activeBoneChannels;
        clip.morphChannelCount = activeMorphChannels;
        clip.matChannelCount = 0; // Динамические каналы материалов настраиваются процедурно
        clip.explicitPadding = 0;

        outClips.push_back(clip);
    }

    std::cout << "[AssetProcessor] Офлайн-модуль анимации полностью укомплектован!" << std::endl;
    std::cout << "  -> Скелетных каналов:       " << outBoneChannels.size() << std::endl;
    std::cout << "  -> Каналов морфинга:        " << outMorphChannels.size() << std::endl;
    std::cout << "  -> Свойств дин. материалов: " << outMatProperties.size() << std::endl;
}


void SceneImporter::writeBlob(
    const std::string& outputPath,
    const std::vector<format::SceneNode>& nodes,
    const std::vector<format::NodeLevelRange>& levels,
    const std::vector<format::MaterialInfo>& materials,
    const std::vector<format::MeshHeader>& meshes,
    const std::vector<format::SkeletonData>& skeletons,
    const std::vector<format::BoneData>& bones,
    const std::vector<format::AnimationClip>& clips,
    const std::vector<format::BoneChannel>& boneChannels,
    const std::vector<format::MorphTarget>& morphTargets,   // ВНЕДРЕНО!
    const std::vector<format::MorphChannel>& morphChannels, // ВНЕДРЕНО!
    const std::vector<format::MaterialProperty>& matProperties, // ВНЕДРЕНО!
    const std::vector<format::MaterialChannel>& matChannels, // ВНЕДРЕНО!
    const std::vector<format::DirectionalLight>& dirLights,
    const std::vector<format::PointLight>& pointLights,
    const std::vector<format::SpotLight>& spotLights,
    const std::vector<float>& keyframeTimes,
    const std::vector<format::AnimationKeyframeValue>& keyframeValues,
    const std::vector<uint8_t>& geometryBulkData,
    const std::vector<format::TextureMetaData>& textureMetas,
    const std::vector<std::vector<uint8_t>>& textureBlobs
) {
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[AssetProcessor] КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать файл " << outputPath << std::endl;
        return;
    }

    format::BlobHeader header{};
    std::memcpy(header.magic, "BLOB", 4);
    header.version = 1;

    uint64_t currentOffset = sizeof(format::BlobHeader);
    auto alignOffset = [](uint64_t offset) -> uint64_t { return (offset + 15) & ~15; };

    // =========================================================================
    // ТОЧНЫЙ РАСЧЕТ СЕТКИ СМЕЩЕНИЙ И СЧЕТЧИКОВ ВСЕХ СЕКЦИЙ
    // =========================================================================

    // Текстуры (таблица метаданных)
    currentOffset = alignOffset(currentOffset);
    header.textureTableOffset = currentOffset;
    header.textureCount = static_cast<uint32_t>(textureMetas.size());
    header.padding0 = 0;
    currentOffset += static_cast<uint64_t>(textureMetas.size()) * sizeof(format::TextureMetaData);

    // Материалы
    currentOffset = alignOffset(currentOffset);
    header.materialTableOffset = currentOffset;
    header.materialCount = static_cast<uint32_t>(materials.size());
    currentOffset += materials.size() * sizeof(format::MaterialInfo);

    // Меши
    currentOffset = alignOffset(currentOffset);
    header.meshTableOffset = currentOffset;
    header.meshCount = static_cast<uint32_t>(meshes.size());
    currentOffset += meshes.size() * sizeof(format::MeshHeader);

    // Скелеты и Кости
    currentOffset = alignOffset(currentOffset);
    header.skeletonTableOffset = currentOffset;
    header.skeletonCount = static_cast<uint32_t>(skeletons.size());
    currentOffset += skeletons.size() * sizeof(format::SkeletonData);

    currentOffset = alignOffset(currentOffset);
    header.boneChannelTableOffset = currentOffset; // Массив уникальных BoneData[]
    header.boneChannelCount = static_cast<uint32_t>(bones.size());
    currentOffset += bones.size() * sizeof(format::BoneData);

    // МОРФИНГ (Учитываем на 100%)
    currentOffset = alignOffset(currentOffset);
    header.morphTargetTableOffset = currentOffset;
    header.morphTargetCount = static_cast<uint32_t>(morphTargets.size());
    currentOffset += morphTargets.size() * sizeof(format::MorphTarget);

    currentOffset = alignOffset(currentOffset);
    header.morphChannelTableOffset = currentOffset;
    header.morphChannelCount = static_cast<uint32_t>(morphChannels.size());
    currentOffset += morphChannels.size() * sizeof(format::MorphChannel);

    // ДИНАМИЧЕСКИЕ МАТЕРИАЛЫ (Учитываем на 100%)
    currentOffset = alignOffset(currentOffset);
    header.matPropertyTableOffset = currentOffset;
    header.matPropertyCount = static_cast<uint32_t>(matProperties.size());
    currentOffset += matProperties.size() * sizeof(format::MaterialProperty);

    currentOffset = alignOffset(currentOffset);
    header.matChannelTableOffset = currentOffset;
    header.matChannelCount = static_cast<uint32_t>(matChannels.size());
    currentOffset += matChannels.size() * sizeof(format::MaterialChannel);

    // Клипы
    currentOffset = alignOffset(currentOffset);
    header.clipTableOffset = currentOffset;
    header.clipCount = static_cast<uint32_t>(clips.size());
    currentOffset += clips.size() * sizeof(format::AnimationClip);

    // Граф сцены
    currentOffset = alignOffset(currentOffset);
    header.sceneGraphOffset = currentOffset;
    header.sceneNodeCount = static_cast<uint32_t>(nodes.size());
    currentOffset += nodes.size() * sizeof(format::SceneNode);

    // Уровни графа
    currentOffset = alignOffset(currentOffset);
    header.nodeLevelRangeTableOffset = currentOffset;
    header.nodeLevelRangeCount = static_cast<uint32_t>(levels.size());
    currentOffset += levels.size() * sizeof(format::NodeLevelRange);

    // Свет
    currentOffset = alignOffset(currentOffset); header.dirLightTableOffset = currentOffset; header.dirLightCount = dirLights.size(); currentOffset += dirLights.size() * sizeof(format::DirectionalLight);
    currentOffset = alignOffset(currentOffset); header.pointLightTableOffset = currentOffset; header.pointLightCount = pointLights.size(); currentOffset += pointLights.size() * sizeof(format::PointLight);
    currentOffset = alignOffset(currentOffset); header.spotLightTableOffset = currentOffset; header.spotLightCount = spotLights.size(); currentOffset += spotLights.size() * sizeof(format::SpotLight);

    // =========================================================================
    // СБОРКА И ВЫРАВНИВАНИЕ МОНОЛИТНОГО BULK DATA BLOB
    // =========================================================================
    std::vector<uint8_t> alignedBulkBlob;

    // Текстуры в bulk-данных будут первыми: считаем их общий размер и смещения внутри bulk'а
    size_t totalTextureBytes = 0;
    std::vector<size_t> textureOffsetsInBulk;
    textureOffsetsInBulk.reserve(textureBlobs.size());
    for (const auto &tb : textureBlobs) {
        textureOffsetsInBulk.push_back(totalTextureBytes);
        totalTextureBytes += static_cast<size_t>(alignOffset(static_cast<uint64_t>(tb.size())));
    }

    size_t boneChannelBytes = boneChannels.size() * sizeof(format::BoneChannel);
    size_t timeBytes         = keyframeTimes.size() * sizeof(float);
    size_t valueBytes        = keyframeValues.size() * sizeof(format::AnimationKeyframeValue);

    size_t alignedBoneChannels = alignOffset(boneChannelBytes);
    size_t alignedTimes        = alignOffset(timeBytes);
    size_t alignedValues       = alignOffset(valueBytes);

    alignedBulkBlob.resize(totalTextureBytes + alignedBoneChannels + alignedTimes + alignedValues + geometryBulkData.size());

    // Копируем текстурные блобы в начало bulk'а
    for (size_t i = 0; i < textureBlobs.size(); ++i) {
        const auto &tb = textureBlobs[i];
        size_t dstOff = textureOffsetsInBulk[i];
        if (!tb.empty()) std::memcpy(alignedBulkBlob.data() + dstOff, tb.data(), tb.size());
        // Нулевая подушка до выравнивания (оставляем ранее инициализированной 0)
    }

    // Далее копируем boneChannels / times / values за текстурами
    size_t afterTextures = totalTextureBytes;
    if (!boneChannels.empty()) std::memcpy(alignedBulkBlob.data() + afterTextures, boneChannels.data(), boneChannelBytes);
    if (!keyframeTimes.empty())  std::memcpy(alignedBulkBlob.data() + afterTextures + alignedBoneChannels, keyframeTimes.data(), timeBytes);
    if (!keyframeValues.empty()) std::memcpy(alignedBulkBlob.data() + afterTextures + alignedBoneChannels + alignedTimes, keyframeValues.data(), valueBytes);
    if (!geometryBulkData.empty())  std::memcpy(alignedBulkBlob.data() + afterTextures + alignedBoneChannels + alignedTimes + alignedValues, geometryBulkData.data(), geometryBulkData.size());

    currentOffset = alignOffset(currentOffset);
    header.bulkDataOffset = currentOffset;
    header.bulkDataSize = alignedBulkBlob.size();
    header.totalFileSize = currentOffset + alignedBulkBlob.size();

    // Перед записью таблиц нужно обновить textureMeta.textureOffset, так как теперь известен bulkDataOffset
    std::vector<format::TextureMetaData> textureMetasCopy = textureMetas;
    for (size_t i = 0; i < textureMetasCopy.size(); ++i) {
        textureMetasCopy[i].textureOffset = header.bulkDataOffset + textureOffsetsInBulk[i];
    }

    // Adjust mesh geometry buffer addresses to absolute file offsets.
    // In globalBulkData layout: [textures | boneChannels | keyframeTimes | keyframeValues | geometry]
    uint64_t geomBase = header.bulkDataOffset
                      + static_cast<uint64_t>(totalTextureBytes)
                      + static_cast<uint64_t>(alignedBoneChannels)
                      + static_cast<uint64_t>(alignedTimes)
                      + static_cast<uint64_t>(alignedValues);
    std::vector<format::MeshHeader> adjustedMeshes = meshes;
    for (auto& m : adjustedMeshes) {
        m.positionBufferAddress  += geomBase;
        m.attributeBufferAddress += geomBase;
        m.indexBufferAddress     += geomBase;
    }

    // =========================================================================
    // ОДНОПРОХОДНАЯ ЗАПИСЬ НА ДИСК
    // =========================================================================
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Явные приведения для stream offsets/sizes чтобы избежать narrowing warnings
    out.seekp(static_cast<std::streamoff>(header.textureTableOffset));
    if (!textureMetasCopy.empty()) out.write(reinterpret_cast<const char*>(textureMetasCopy.data()), static_cast<std::streamsize>(textureMetasCopy.size() * sizeof(format::TextureMetaData)));

    out.seekp(static_cast<std::streamoff>(header.materialTableOffset));
    out.write(reinterpret_cast<const char*>(materials.data()), static_cast<std::streamsize>(materials.size() * sizeof(format::MaterialInfo)));
    out.seekp(static_cast<std::streamoff>(header.meshTableOffset));
    out.write(reinterpret_cast<const char*>(adjustedMeshes.data()), static_cast<std::streamsize>(adjustedMeshes.size() * sizeof(format::MeshHeader)));

    out.seekp(static_cast<std::streamoff>(header.skeletonTableOffset));
    out.write(reinterpret_cast<const char*>(skeletons.data()), static_cast<std::streamsize>(skeletons.size() * sizeof(format::SkeletonData)));

    out.seekp(static_cast<std::streamoff>(header.boneChannelTableOffset));
    out.write(reinterpret_cast<const char*>(bones.data()), static_cast<std::streamsize>(bones.size() * sizeof(format::BoneData)));

    // Физически пишем морфинг и дин. материалы на диск!
    out.seekp(static_cast<std::streamoff>(header.morphTargetTableOffset));
    out.write(reinterpret_cast<const char*>(morphTargets.data()), static_cast<std::streamsize>(morphTargets.size() * sizeof(format::MorphTarget)));

    out.seekp(static_cast<std::streamoff>(header.morphChannelTableOffset));
    out.write(reinterpret_cast<const char*>(morphChannels.data()), static_cast<std::streamsize>(morphChannels.size() * sizeof(format::MorphChannel)));

    out.seekp(static_cast<std::streamoff>(header.matPropertyTableOffset));
    out.write(reinterpret_cast<const char*>(matProperties.data()), static_cast<std::streamsize>(matProperties.size() * sizeof(format::MaterialProperty)));

    out.seekp(static_cast<std::streamoff>(header.matChannelTableOffset));
    out.write(reinterpret_cast<const char*>(matChannels.data()), static_cast<std::streamsize>(matChannels.size() * sizeof(format::MaterialChannel)));

    out.seekp(static_cast<std::streamoff>(header.clipTableOffset));
    out.write(reinterpret_cast<const char*>(clips.data()), static_cast<std::streamsize>(clips.size() * sizeof(format::AnimationClip)));

    out.seekp(static_cast<std::streamoff>(header.sceneGraphOffset));
    out.write(reinterpret_cast<const char*>(nodes.data()), static_cast<std::streamsize>(nodes.size() * sizeof(format::SceneNode)));

    out.seekp(static_cast<std::streamoff>(header.nodeLevelRangeTableOffset));
    out.write(reinterpret_cast<const char*>(levels.data()), static_cast<std::streamsize>(levels.size() * sizeof(format::NodeLevelRange)));

    out.seekp(static_cast<std::streamoff>(header.dirLightTableOffset));
    out.write(reinterpret_cast<const char*>(dirLights.data()), static_cast<std::streamsize>(dirLights.size() * sizeof(format::DirectionalLight)));

    out.seekp(static_cast<std::streamoff>(header.pointLightTableOffset));
    out.write(reinterpret_cast<const char*>(pointLights.data()), static_cast<std::streamsize>(pointLights.size() * sizeof(format::PointLight)));

    out.seekp(static_cast<std::streamoff>(header.spotLightTableOffset));
    out.write(reinterpret_cast<const char*>(spotLights.data()), static_cast<std::streamsize>(spotLights.size() * sizeof(format::SpotLight)));

    out.seekp(static_cast<std::streamoff>(header.bulkDataOffset));
    out.write(reinterpret_cast<const char*>(alignedBulkBlob.data()), static_cast<std::streamsize>(alignedBulkBlob.size()));
    out.close();

    std::cout << "[AssetProcessor] КОНВЕЙЕР ИДЕАЛЕН. Все 14 таблиц запечены!" << std::endl;
}



bool SceneImporter::loadScene(std::string const& inputPath, std::string const& outputPath) {
    std::cout << "[AssetProcessor] Начало сквозной компиляции сцены: " << inputPath << std::endl;

    // 1. Инициализируем импортер Assimp с жесткими флагами оптимизации сеток
    Assimp::Importer assimpImporter;
    unsigned int flags = aiProcess_FlipUVs |
                         aiProcess_Triangulate |
                         aiProcess_GenSmoothNormals |
                         aiProcess_CalcTangentSpace |
                         aiProcess_ImproveCacheLocality;

    const aiScene* scene = assimpImporter.ReadFile(inputPath, flags);
    if (!scene) {
        std::cerr << "[AssetProcessor] КРИТИЧЕСКАЯ ОШИБКА Assimp: " << assimpImporter.GetErrorString() << std::endl;
        return false;
    }

    // 2. Создаем плоские буферы под наш 272-байтовый BlobHeader
    std::vector<format::SceneNode>               nodes;
    std::vector<format::NodeLevelRange>          levels;
    std::vector<format::MaterialInfo>            materials;
    std::vector<format::MeshHeader>              meshes;
    std::vector<format::SkeletonData>            skeletons;
    std::vector<format::BoneData>                bones;
    std::vector<format::AnimationClip>           clips;
    std::vector<format::BoneChannel>             boneChannels;
    // Морфинг и динамические материалы
    std::vector<format::MorphTarget>             morphTargets;
    std::vector<format::MorphChannel>            morphChannels;
    std::vector<format::MaterialProperty>        matProperties;
    std::vector<format::MaterialChannel>         matChannels;
    std::vector<format::DirectionalLight>        dirLights;
    std::vector<format::PointLight>              pointLights;
    std::vector<format::SpotLight>               spotLights;
    std::vector<float>                           keyframeTimes;
    std::vector<format::AnimationKeyframeValue>  keyframeValues;
    std::vector<uint8_t>                         globalBulkData;

    // Список путей скомпилированных текстур (для Bindless-индексации материалов)
    std::vector<std::string> compiledTexturePaths;

    // =========================================================================
    // ПОСЛЕДОВАТЕЛЬНЫЙ ЗАПУСК ВСЕХ МОДУЛЕЙ ОФЛАЙН-КОНВЕЙЕРА
    // =========================================================================

    // Шаг А: Строим плоский поуровневый граф нод со схлопыванием статических пустышек
    parseSceneGraph(scene, nodes, levels);

    // Шаг Б: Собираем PBR-материалы со сквозным поиском ORM-карт
    parseMaterials(scene, std::filesystem::absolute(inputPath).parent_path(), compiledTexturePaths, materials);

    // Шаг Б2: Упаковываем найденные пути текстур (включая embedded://) в бинарные чанки с компрессией
    std::vector<format::TextureMetaData> textureMetas;
    std::vector<std::vector<uint8_t>> textureBlobs;
    textureMetas.reserve(compiledTexturePaths.size());
    textureBlobs.reserve(compiledTexturePaths.size());

    using namespace shuttle_engine::compiler;
    TextureImportOptions defaultOptions;
    defaultOptions.generateMips = true;
    defaultOptions.flipY = false;
    defaultOptions.format = TextureFormat::BC7_SRGB;

    // Prepare texture importer (init encoders)
    TextureImporter::
        initialize();

    auto toLower = [](const std::string &s){ std::string r=s; for (auto &c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return r; };

    size_t texCount = compiledTexturePaths.size();
    textureMetas.resize(texCount);
    textureBlobs.resize(texCount);

    #pragma omp parallel for default(none) shared(compiledTexturePaths, textureMetas, textureBlobs, scene, std::cout, std::cerr, defaultOptions, texCount, toLower)
    for (int ti = 0; ti < static_cast<int>(texCount); ++ti) {
        const std::string& p = compiledTexturePaths[ti];
        format::TextureMetaData meta{};
        std::vector<uint8_t> blob;

        if (p.rfind("embedded://", 0) == 0) {
            try {
                if (int embIdx = std::stoi(p.substr(11)); embIdx >= 0 && static_cast<uint32_t>(embIdx) < scene->mNumTextures) {
                    if (aiTexture* tex = scene->mTextures[embIdx]; tex->mHeight == 0 && tex->pcData) {
                        const auto* mem = reinterpret_cast<const uint8_t*>(tex->pcData);
                        size_t memSize = tex->mWidth;

                        std::string hint = "png";
                        if (memSize >= 8 && mem[0] == 0x89 && mem[1] == 'P' && mem[2] == 'N' && mem[3] == 'G') hint = "png";
                        else if (memSize >= 3 && mem[0] == 0xFF && mem[1] == 0xD8) hint = "jpg";
                        else if (memSize >= 4 && mem[0] == 'D' && mem[1] == 'D' && mem[2] == 'S') hint = "dds";

                        std::string fname = (tex->mFilename.length > 0) ? std::string(tex->mFilename.C_Str()) : p;
                        auto lname = toLower(fname);
                        TextureImportOptions opts = defaultOptions;
                        if (lname.find("normal") != std::string::npos) opts.format = TextureFormat::BC5_UNORM;

                        if (!TextureImporter::import(mem, memSize, hint, opts, meta, blob)) {
                            #pragma omp critical
                            std::cerr << "[AssetProcessor WARNING] Failed to import embedded texture: " << p << std::endl;
                        }
                    } else if (tex->mHeight > 0 && tex->pcData) {
                        int w = static_cast<int>(tex->mWidth);
                        int h = static_cast<int>(tex->mHeight);
                        const auto* rgba = reinterpret_cast<const uint8_t*>(tex->pcData);
                        std::string fname = (tex->mFilename.length > 0) ? std::string(tex->mFilename.C_Str()) : p;
                        auto lname = toLower(fname);
                        TextureImportOptions opts = defaultOptions;
                        if (lname.find("normal") != std::string::npos) opts.format = TextureFormat::BC5_UNORM;

                        if (!TextureImporter::importFromRGBA(rgba, w, h, opts, meta, blob)) {
                            #pragma omp critical
                            std::cerr << "[AssetProcessor WARNING] Failed to import embedded RGBA texture: " << p << std::endl;
                        }
                    } else {
                        #pragma omp critical
                        std::cerr << "[AssetProcessor WARNING] Embedded texture missing data: " << p << std::endl;
                    }
                }
            } catch (...) {
                #pragma omp critical
                std::cerr << "[AssetProcessor WARNING] Bad embedded texture ref: " << p << std::endl;
            }
        } else {
            std::string ext;
            if (size_t pos = p.find_last_of('.'); pos != std::string::npos) ext = p.substr(pos + 1);
            else ext = "";

            TextureImportOptions opts = defaultOptions;
            if (auto lowerName = toLower(p); lowerName.find("normal") != std::string::npos) opts.format = TextureFormat::BC5_UNORM;

            if (std::filesystem::exists(p)) {
                if (!TextureImporter::import(p, opts, meta, blob)) {
                    #pragma omp critical
                    std::cerr << "[AssetProcessor WARNING] Failed to import texture on disk: " << p << std::endl;
                }
            } else {
                #pragma omp critical
                std::cerr << "[AssetProcessor WARNING] Texture file does not exist: " << p << std::endl;
            }
        }

        textureMetas[ti] = meta;
        textureBlobs[ti] = std::move(blob);
    }

    // Шаг В: Оптимизируем геометрию через meshoptimizer, бьем на потоки и строим ЛОДы
    processGeometry(scene, meshes, globalBulkData);

    // Шаг Г: Сортируем источники света под Forward+/Clustered закраску
    parseLights(scene, dirLights, pointLights, spotLights);

    // Шаг Д: Вытягиваем кости скелета, инверсные матрицы связывания и ключевые кадры анимации
    parseAnimations(
        scene,
        nodes,
        skeletons,
        bones,
        clips,
        boneChannels,
        morphTargets,
        morphChannels,
        matProperties,
        matChannels,
        keyframeTimes,
        keyframeValues
    );

    // Шаг Е: Финальное 16-байтовое выравнивание всех таблиц и запекание монолитного файла .scene
    writeBlob(
        outputPath,
        nodes,
        levels,
        materials,
        meshes,
        skeletons,
        bones,
        clips,
        boneChannels,
        morphTargets,
        morphChannels,
        matProperties,
        matChannels,
        dirLights,
        pointLights,
        spotLights,
        keyframeTimes,
        keyframeValues,
        globalBulkData,
        textureMetas,
        textureBlobs
    );

    std::cout << "[AssetProcessor] КОНВЕЙЕР УСПЕШНО СРАБОТАЛ. Файл запечен: " << outputPath << std::endl;
    return true;
}
}
