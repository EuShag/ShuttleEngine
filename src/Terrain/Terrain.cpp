//
// Created by Shagu on 25.05.2026.
//

#include "Terrain.hpp"
#include <glm/glm.hpp>

namespace shuttle_engine {
    HostMeshData TerrainGeometryGenerator::createFromHeightMap(const TerrainProperties& props, const Image1D16bit& heightMap) { // Изменил Image1D16bit на Image
        HostMeshData mesh;
        uint32_t width = props.meshResolution.width;  // Используем meshResolution
        uint32_t height = props.meshResolution.height; // Используем meshResolution

        // Проверки на минимальный размер
        if (width < 2 || height < 2) {
            throw std::runtime_error("Terrain resolution must be at least 2x2 for geometry generation.");
        }

        // Расчет размера одной ячейки
        float gridCellSizeX = props.worldSize.x / static_cast<float>(width - 1);
        float gridCellSizeY = props.worldSize.y / static_cast<float>(height - 1);

        // Расчет общего количества вершин и индексов
        uint32_t totalVertices = width * height;
        uint32_t totalIndices = (width - 1) * (height - 1) * 6;

        // 1. Резервируем и ресайзим память
        mesh.positions.resize(totalVertices);
        mesh.normals.resize(totalVertices);
        mesh.uvs.resize(totalVertices);
        mesh.tangents.resize(totalVertices); // Теперь тут будут vec4
        mesh.indices.reserve(totalIndices);

        // Временные аккумуляторы для нормалей, тангенсов и битангенсов
        std::vector<glm::vec3> normalAccumulator(totalVertices, glm::vec3(0.0f));
        std::vector<glm::vec3> tangentAccumulator(totalVertices, glm::vec3(0.0f));
        std::vector<glm::vec3> bitangentAccumulator(totalVertices, glm::vec3(0.0f)); // Добавляем аккумулятор для битангентов


        // 2. Генерация вершин (позиции и UV)
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t idx = y * width + x;

                float u_sample = static_cast<float>(x) / static_cast<float>(width - 1);
                float v_sample = static_cast<float>(y) / static_cast<float>(height - 1);

                float heightValue = heightMap.sampleBilinear(u_sample, v_sample);
                float worldHeight = props.minHeight + heightValue * (props.maxHeight - props.minHeight);

                mesh.positions[idx] = glm::vec3(
                    x * gridCellSizeX - props.worldSize.x / 2.0f,
                    worldHeight,
                    y * gridCellSizeY - props.worldSize.y / 2.0f
                );

                mesh.uvs[idx] = glm::vec2(
                    u_sample * props.textureRepeatFactor.x,
                    v_sample * props.textureRepeatFactor.y
                );
            }
        }

        // 3. Генерация индексов
        for (uint32_t y = 0; y < height - 1; ++y) {
            for (uint32_t x = 0; x < width - 1; ++x) {
                uint32_t i0 = y * width + x;             // TopLeft
                uint32_t i1 = y * width + x + 1;         // TopRight
                uint32_t i2 = (y + 1) * width + x;       // BottomLeft
                uint32_t i3 = (y + 1) * width + x + 1;   // BottomRight

                // Первый треугольник: TopLeft -> BottomLeft -> BottomRight (CCW)
                mesh.indices.push_back(i0);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i3);

                // Второй треугольник: TopLeft -> BottomRight -> TopRight (CCW)
                mesh.indices.push_back(i0);
                mesh.indices.push_back(i3);
                mesh.indices.push_back(i1);
            }
        }

        // 4. Расчет нормалей, тангенсов и битангенсов (аккумуляция)
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            uint32_t i0 = mesh.indices[i + 0];
            uint32_t i1 = mesh.indices[i + 1];
            uint32_t i2 = mesh.indices[i + 2];

            const glm::vec3& v0 = mesh.positions[i0];
            const glm::vec3& v1 = mesh.positions[i1];
            const glm::vec3& v2 = mesh.positions[i2];

            const glm::vec2& uv0 = mesh.uvs[i0];
            const glm::vec2& uv1 = mesh.uvs[i1];
            const glm::vec2& uv2 = mesh.uvs[i2];

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;

            float du1 = uv1.x - uv0.x;
            float dv1 = uv1.y - uv0.y;
            float du2 = uv2.x - uv0.x;
            float dv2 = uv2.y - uv0.y;

            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2)); // Нормализуем сразу

            // Добавляем нормаль к аккумуляторам вершин треугольника
            normalAccumulator[i0] += faceNormal;
            normalAccumulator[i1] += faceNormal;
            normalAccumulator[i2] += faceNormal;

            float det = (du1 * dv2 - dv1 * du2);
            if (std::abs(det) > 1e-6f) {
                float f = 1.0f / det;

                glm::vec3 tangent;
                tangent.x = f * (dv2 * edge1.x - dv1 * edge2.x);
                tangent.y = f * (dv2 * edge1.y - dv1 * edge2.y);
                tangent.z = f * (dv2 * edge1.z - dv1 * edge2.z);
                tangent = glm::normalize(tangent); // Нормализуем тангент

                glm::vec3 bitangent; // Также вычисляем битангент
                bitangent.x = f * (-du2 * edge1.x + du1 * edge2.x);
                bitangent.y = f * (-du2 * edge1.y + du1 * edge2.y);
                bitangent.z = f * (-du2 * edge1.z + du1 * edge2.z);
                bitangent = glm::normalize(bitangent); // Нормализуем битангент

                tangentAccumulator[i0] += tangent;
                tangentAccumulator[i1] += tangent;
                tangentAccumulator[i2] += tangent;

                bitangentAccumulator[i0] += bitangent; // Аккумулятор для битангентов
                bitangentAccumulator[i1] += bitangent;
                bitangentAccumulator[i2] += bitangent;
            }
        }

        // 5. Финализация нормалей и тангентов (нормализация, Грама-Шмидта и вычисление W)
        for (uint32_t i = 0; i < totalVertices; ++i) {
            glm::vec3 n = glm::normalize(normalAccumulator[i]);
            glm::vec3 t = glm::normalize(tangentAccumulator[i]);
            glm::vec3 b = glm::normalize(bitangentAccumulator[i]); // Теперь у нас есть усредненный битангент

            // Если тангент нулевой (например, на плоском участке без UV-разницы), задаем базовый
            if (glm::length(t) < 1e-6f) { // В случае, если тангент не был посчитан
                t = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            // Процесс Грама-Шмидта: делаем тангент строго перпендикулярным нормали
            // t' = normalize(t - n * dot(n, t))
            glm::vec3 orthonormalTangent = glm::normalize(t - glm::dot(t, n) * n);

            // Вычисление W (handedness)
            // Сравниваем векторное произведение N x T с усредненным битангентом B
            // Если N x T и B сонаправлены, W = 1.0, иначе -1.0
            float w = (glm::dot(glm::cross(n, orthonormalTangent), b) < 0.0f) ? -1.0f : 1.0f;

            mesh.normals[i] = n;
            mesh.tangents[i] = glm::vec4(orthonormalTangent, w); // Сохраняем как vec4
        }

        return mesh;
    }
}