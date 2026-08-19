#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

namespace shuttle::engine::ibl
{
    inline constexpr float Pi = 3.14159265358979323846f;
    inline constexpr float TwoPi = Pi * 2.0f;
    inline constexpr float InvPi = 1.0f / Pi;

    // ============================================================
    // Images & Cubemaps
    // ============================================================

    struct Image2D
    {
        uint32_t width{};
        uint32_t height{};
        std::vector<glm::vec4> pixels{};

        [[nodiscard]] glm::vec4& at(uint32_t x, uint32_t y) { return pixels[y * width + x]; }
        [[nodiscard]] glm::vec4 const& at(uint32_t x, uint32_t y) const { return pixels[y * width + x]; }
    };

    enum class CubemapFace : uint32_t
    {
        PositiveX = 0, NegativeX = 1,
        PositiveY = 2, NegativeY = 3,
        PositiveZ = 4, NegativeZ = 5
    };

    struct CubemapMip
    {
        uint32_t size{};
        std::array<std::vector<glm::vec4>, 6> faces{};
    };

    struct Cubemap
    {
        std::vector<CubemapMip> mips{};
    };

    // ============================================================
    // Helpers & Parallel Execution
    // ============================================================

    inline float saturate(float v) { return std::clamp(v, 0.0f, 1.0f); }

    inline float distributionGGX(float nDotH, float roughness)
    {
        float a = roughness * roughness;
        float a2 = a * a;
        float denom = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
        return a2 / std::max(Pi * denom * denom, 1e-7f);
    }

    template<typename F>
    void parallelFor(uint64_t count, F&& function)
    {
        uint32_t threadCount = std::max(1u, std::thread::hardware_concurrency());
        std::atomic<uint64_t> cursor{0};
        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        for (uint32_t i = 0; i < threadCount; ++i)
        {
            threads.emplace_back([&]() {
                for (;;)
                {
                    uint64_t index = cursor.fetch_add(1, std::memory_order_relaxed);
                    if (index >= count) break;
                    function(index);
                }
            });
        }
        for (std::thread& t : threads) t.join();
    }

    inline float cubemapTexelSolidAngle(uint32_t x, uint32_t y, uint32_t size)
    {
        float const invRes = 1.0f / static_cast<float>(size);
        float const u = 2.0f * (static_cast<float>(x) + 0.5f) * invRes - 1.0f;
        float const v = 2.0f * (static_cast<float>(y) + 0.5f) * invRes - 1.0f;

        float const x0 = u - invRes;
        float const y0 = v - invRes;
        float const x1 = u + invRes;
        float const y1 = v + invRes;

        auto areaElement = [](float a, float b) {
            return std::atan2(a * b, std::sqrt(a * a + b * b + 1.0f));
        };

        return areaElement(x0, y0) - areaElement(x0, y1) - areaElement(x1, y0) + areaElement(x1, y1);
    }

    // ============================================================
    // Sampling & Cubemap Math
    // ============================================================

    inline glm::vec3 directionFromCubemapTexel(CubemapFace face, uint32_t x, uint32_t y, uint32_t size)
    {
        float const u = 2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size) - 1.0f;
        float const v = 2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size) - 1.0f;

        switch (face)
        {
            case CubemapFace::PositiveX: return glm::normalize(glm::vec3(1.0f, -v, -u));
            case CubemapFace::NegativeX: return glm::normalize(glm::vec3(-1.0f, -v, u));
            case CubemapFace::PositiveY: return glm::normalize(glm::vec3(u, 1.0f, v));
            case CubemapFace::NegativeY: return glm::normalize(glm::vec3(u, -1.0f, -v));
            case CubemapFace::PositiveZ: return glm::normalize(glm::vec3(u, -v, 1.0f));
            case CubemapFace::NegativeZ: return glm::normalize(glm::vec3(-u, -v, -1.0f));
        }
        return {0.0f, 1.0f, 0.0f};
    }

    inline glm::vec4 sampleEquirectangular(Image2D const& image, glm::vec3 direction)
    {
        direction = glm::normalize(direction);
        float u = std::atan2(direction.z, direction.x) / TwoPi + 0.5f;
        float v = std::acos(std::clamp(direction.y, -1.0f, 1.0f)) / Pi;

        u -= std::floor(u);
        v = std::clamp(v, 0.0f, 1.0f);

        float fx = u * static_cast<float>(image.width - 1);
        float fy = v * static_cast<float>(image.height - 1);

        auto x0 = static_cast<uint32_t>(std::floor(fx));
        auto y0 = static_cast<uint32_t>(std::floor(fy));
        uint32_t x1 = (x0 + 1) % image.width;
        uint32_t y1 = std::min(y0 + 1, image.height - 1);

        float tx = fx - static_cast<float>(x0);
        float ty = fy - static_cast<float>(y0);

        glm::vec3 c00(image.at(x0, y0));
        glm::vec3 c10(image.at(x1, y0));
        glm::vec3 c01(image.at(x0, y1));
        glm::vec3 c11(image.at(x1, y1));

        glm::vec3 c0 = glm::mix(c00, c10, tx);
        glm::vec3 c1 = glm::mix(c01, c11, tx);

        return {glm::mix(c0, c1, ty), 1.0f};
    }

    inline CubemapFace majorAxisFace(glm::vec3 d)
    {
        float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
        if (ax >= ay && ax >= az) return d.x >= 0.0f ? CubemapFace::PositiveX : CubemapFace::NegativeX;
        if (ay >= ax && ay >= az) return d.y >= 0.0f ? CubemapFace::PositiveY : CubemapFace::NegativeY;
        return d.z >= 0.0f ? CubemapFace::PositiveZ : CubemapFace::NegativeZ;
    }

    inline glm::vec4 sampleCubemapMip(Cubemap const& cubemap, uint32_t mipIndex, glm::vec3 direction)
    {
        direction = glm::normalize(direction);
        mipIndex = std::min<uint32_t>(mipIndex, static_cast<uint32_t>(cubemap.mips.size() - 1));

        CubemapMip const& mip = cubemap.mips[mipIndex];
        CubemapFace face = majorAxisFace(direction);

        float u = 0.0f, v = 0.0f;
        float ax = std::abs(direction.x), ay = std::abs(direction.y), az = std::abs(direction.z);

        switch (face)
        {
            case CubemapFace::PositiveX: u = -direction.z / ax; v = -direction.y / ax; break;
            case CubemapFace::NegativeX: u =  direction.z / ax; v = -direction.y / ax; break;
            case CubemapFace::PositiveY: u =  direction.x / ay; v =  direction.z / ay; break;
            case CubemapFace::NegativeY: u =  direction.x / ay; v = -direction.z / ay; break;
            case CubemapFace::PositiveZ: u =  direction.x / az; v = -direction.y / az; break;
            case CubemapFace::NegativeZ: u = -direction.x / az; v = -direction.y / az; break;
        }

        float fx = (u * 0.5f + 0.5f) * static_cast<float>(mip.size - 1);
        float fy = (v * 0.5f + 0.5f) * static_cast<float>(mip.size - 1);

        uint32_t x0 = static_cast<uint32_t>(std::floor(std::clamp(fx, 0.0f, static_cast<float>(mip.size - 1))));
        uint32_t y0 = static_cast<uint32_t>(std::floor(std::clamp(fy, 0.0f, static_cast<float>(mip.size - 1))));
        uint32_t x1 = std::min(x0 + 1, mip.size - 1);
        uint32_t y1 = std::min(y0 + 1, mip.size - 1);

        float tx = fx - static_cast<float>(x0);
        float ty = fy - static_cast<float>(y0);

        auto const& pixels = mip.faces[static_cast<uint32_t>(face)];

        glm::vec3 c00(pixels[y0 * mip.size + x0]);
        glm::vec3 c10(pixels[y0 * mip.size + x1]);
        glm::vec3 c01(pixels[y1 * mip.size + x0]);
        glm::vec3 c11(pixels[y1 * mip.size + x1]);

        glm::vec3 c0 = glm::mix(c00, c10, tx);
        glm::vec3 c1 = glm::mix(c01, c11, tx);

        return {glm::mix(c0, c1, ty), 1.0f};
    }

    inline glm::vec4 sampleCubemap(Cubemap const& cubemap, glm::vec3 direction)
    {
        return sampleCubemapMip(cubemap, 0, direction);
    }

    inline glm::vec4 sampleCubemapLod(Cubemap const& cubemap, glm::vec3 direction, float lod)
    {
        if (cubemap.mips.empty()) return {};

        auto maxLod = static_cast<float>(cubemap.mips.size() - 1);
        lod = std::clamp(lod, 0.0f, maxLod);

        auto mip0 = static_cast<uint32_t>(std::floor(lod));
        uint32_t mip1 = std::min<uint32_t>(mip0 + 1u, static_cast<uint32_t>(cubemap.mips.size() - 1));
        float t = lod - static_cast<float>(mip0);

        glm::vec3 c0(sampleCubemapMip(cubemap, mip0, direction));
        glm::vec3 c1(sampleCubemapMip(cubemap, mip1, direction));

        return {glm::mix(c0, c1, t), 1.0f};
    }

    inline Cubemap generateCubemapMipChain(Cubemap const& baseCubemap)
    {
        Cubemap result{};
        if (baseCubemap.mips.empty()) return result;

        result.mips.push_back(baseCubemap.mips[0]);
        uint32_t currentSize = baseCubemap.mips[0].size;

        while (currentSize > 1)
        {
            uint32_t nextSize = std::max(1u, currentSize / 2u);
            CubemapMip nextMip{};
            nextMip.size = nextSize;

            for (auto& face : nextMip.faces)
            {
                face.resize(static_cast<size_t>(nextSize) * nextSize);
            }

            auto previousMipIndex = static_cast<uint32_t>(result.mips.size() - 1);
            Cubemap temp{};
            temp.mips = result.mips;

            uint64_t const totalTexels = 6ULL * nextSize * nextSize;

            parallelFor(totalTexels, [&temp, previousMipIndex, nextSize, &nextMip](uint64_t linearIndex) {
                auto faceIndex = static_cast<uint32_t>(linearIndex / (static_cast<uint64_t>(nextSize) * nextSize));
                auto texelIndex = static_cast<uint32_t>(linearIndex % (static_cast<uint64_t>(nextSize) * nextSize));

                glm::vec3 direction = directionFromCubemapTexel(static_cast<CubemapFace>(faceIndex), texelIndex % nextSize, texelIndex / nextSize, nextSize);
                nextMip.faces[faceIndex][texelIndex] = sampleCubemapMip(temp, previousMipIndex, direction);
            });

            result.mips.push_back(std::move(nextMip));
            currentSize = nextSize;
        }

        return result;
    }

    // ============================================================
    // Low Discrepancy Sequence & Sampling
    // ============================================================

    inline float radicalInverseVdC(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    inline glm::vec2 hammersley(uint32_t i, uint32_t sampleCount)
    {
        return { static_cast<float>(i) / static_cast<float>(sampleCount), radicalInverseVdC(i) };
    }

    inline void makeBasis(glm::vec3 n, glm::vec3& tangent, glm::vec3& bitangent)
    {
        glm::vec3 up = std::abs(n.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        tangent = glm::normalize(glm::cross(up, n));
        bitangent = glm::cross(n, tangent);
    }

    inline glm::vec3 importanceSampleGGX(glm::vec2 xi, float roughness, glm::vec3 n)
    {
        float a = roughness * roughness;
        float phi = TwoPi * xi.x;
        float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

        glm::vec3 h(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

        glm::vec3 tangent, bitangent;
        makeBasis(n, tangent, bitangent);

        return glm::normalize(tangent * h.x + bitangent * h.y + n * h.z);
    }

    inline glm::vec3 cosineSampleHemisphere(glm::vec2 xi, glm::vec3 n)
    {
        float phi = TwoPi * xi.x;
        float cosTheta = std::sqrt(1.0f - xi.y);
        float sinTheta = std::sqrt(xi.y);

        glm::vec3 local(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

        glm::vec3 tangent, bitangent;
        makeBasis(n, tangent, bitangent);

        return glm::normalize(tangent * local.x + bitangent * local.y + n * local.z);
    }

    // ============================================================
    // Generation Pipelines
    // ============================================================

    inline Cubemap equirectangularToCubemap(Image2D const& hdr, uint32_t cubemapSize)
    {
        Cubemap result{};
        result.mips.resize(1);

        CubemapMip& mip = result.mips[0];
        mip.size = cubemapSize;

        for (auto& face : mip.faces)
        {
            face.resize(static_cast<uint64_t>(cubemapSize) * cubemapSize);
        }

        uint64_t const totalTexels = static_cast<uint64_t>(6) * cubemapSize * cubemapSize;

        parallelFor(totalTexels, [&hdr, cubemapSize, &mip](uint64_t linearIndex) {
            auto faceIndex = static_cast<uint32_t>(linearIndex / (static_cast<uint64_t>(cubemapSize) * cubemapSize));
            auto const texelIndex = static_cast<uint32_t>(linearIndex % (static_cast<uint64_t>(cubemapSize) * cubemapSize));

            glm::vec3 const direction = directionFromCubemapTexel(static_cast<CubemapFace>(faceIndex), texelIndex % cubemapSize, texelIndex / cubemapSize, cubemapSize);
            mip.faces[faceIndex][texelIndex] = sampleEquirectangular(hdr, direction);
        });

        return result;
    }

    inline Cubemap generateIrradianceCubemap(Cubemap const& environment, uint32_t irradianceSize, uint32_t /*sampleCount*/ = 0)
    {
        Cubemap result{};
        result.mips.resize(1);

        CubemapMip& outMip = result.mips[0];
        outMip.size = irradianceSize;

        for (auto& face : outMip.faces)
        {
            face.resize(static_cast<uint64_t>(irradianceSize) * irradianceSize);
        }

        if (environment.mips.empty()) return result;

        CubemapMip const& sourceMip = environment.mips[0];
        uint32_t sourceSize = sourceMip.size;
        uint64_t const totalOutputTexels = static_cast<uint64_t>(6) * irradianceSize * irradianceSize;

        parallelFor(totalOutputTexels, [&sourceMip, sourceSize, irradianceSize, &outMip](uint64_t linearIndex) {
            auto outFaceIndex = static_cast<uint32_t>(linearIndex / (static_cast<uint64_t>(irradianceSize) * irradianceSize));
            auto outTexelIndex = static_cast<uint32_t>(linearIndex % (static_cast<uint64_t>(irradianceSize) * irradianceSize));

            glm::vec3 normal = directionFromCubemapTexel(static_cast<CubemapFace>(outFaceIndex), outTexelIndex % irradianceSize, outTexelIndex / irradianceSize, irradianceSize);
            glm::vec3 accumulated(0.0f);

            for (uint32_t inFaceIndex = 0; inFaceIndex < 6; ++inFaceIndex)
            {
                auto const& inFace = sourceMip.faces[inFaceIndex];
                for (uint32_t inY = 0; inY < sourceSize; ++inY)
                {
                    for (uint32_t inX = 0; inX < sourceSize; ++inX)
                    {
                        glm::vec3 lightDirection = directionFromCubemapTexel(static_cast<CubemapFace>(inFaceIndex), inX, inY, sourceSize);
                        float const nDotL = std::max(glm::dot(normal, lightDirection), 0.0f);
                        if (nDotL <= 0.0f) continue;

                        float const solidAngle = cubemapTexelSolidAngle(inX, inY, sourceSize);
                        glm::vec3 radiance(inFace[static_cast<size_t>(inY) * sourceSize + inX]);
                        accumulated += radiance * (nDotL * solidAngle);
                    }
                }
            }

            outMip.faces[outFaceIndex][outTexelIndex] = glm::vec4(accumulated * InvPi, 1.0f);
        });

        return result;
    }

    inline uint32_t calculateMipCount(uint32_t size)
    {
        uint32_t count = 1;
        while (size > 1) { size /= 2; ++count; }
        return count;
    }

    inline Cubemap generatePrefilteredGGXCubemap(
        Cubemap const& environmentWithMips,
        uint32_t baseSize,
        uint32_t mipCount = 0,
        uint32_t sampleCount = 1024)
    {
        if (environmentWithMips.mips.empty()) return {};
        if (mipCount == 0) mipCount = calculateMipCount(baseSize);

        Cubemap result{};
        result.mips.resize(mipCount);

        uint32_t sourceBaseSize = environmentWithMips.mips[0].size;
        float sourceTexelSolidAngle = 4.0f * Pi / (6.0f * static_cast<float>(sourceBaseSize * sourceBaseSize));

        for (uint32_t mipIndex = 0; mipIndex < mipCount; ++mipIndex)
        {
            uint32_t mipSize = std::max(1u, baseSize >> mipIndex);
            float roughness = mipCount <= 1 ? 0.0f : static_cast<float>(mipIndex) / static_cast<float>(mipCount - 1);

            CubemapMip& mip = result.mips[mipIndex];
            mip.size = mipSize;

            for (auto& face : mip.faces)
            {
                face.resize(static_cast<uint64_t>(mipSize) * mipSize);
            }

            uint64_t totalTexels = 6ULL * mipSize * mipSize;

            if (mipIndex == 0)
            {
                parallelFor(totalTexels, [mipSize, &mip, &environmentWithMips](uint64_t linearIndex) {
                    auto faceIndex = static_cast<uint32_t>(linearIndex / (static_cast<uint64_t>(mipSize) * mipSize));
                    auto texelIndex = static_cast<uint32_t>(linearIndex % (static_cast<uint64_t>(mipSize) * mipSize));

                    glm::vec3 direction = directionFromCubemapTexel(static_cast<CubemapFace>(faceIndex), texelIndex % mipSize, texelIndex / mipSize, mipSize);
                    mip.faces[faceIndex][texelIndex] = sampleCubemapLod(environmentWithMips, direction, 0.0f);
                });
                continue;
            }

            parallelFor(totalTexels, [&environmentWithMips, mipSize, roughness, sampleCount, sourceTexelSolidAngle, &mip](uint64_t linearIndex) {
                auto faceIndex = static_cast<uint32_t>(linearIndex / (static_cast<uint64_t>(mipSize) * mipSize));
                auto texelIndex = static_cast<uint32_t>(linearIndex % (static_cast<uint64_t>(mipSize) * mipSize));

                glm::vec3 N = directionFromCubemapTexel(static_cast<CubemapFace>(faceIndex), texelIndex % mipSize, texelIndex / mipSize, mipSize);
                glm::vec3 V = N;
                glm::vec3 prefiltered(0.0f);
                float totalWeight = 0.0f;

                for (uint32_t i = 0; i < sampleCount; ++i)
                {
                    glm::vec2 Xi = hammersley(i, sampleCount);
                    glm::vec3 H  = importanceSampleGGX(Xi, roughness, N);
                    glm::vec3 L  = glm::normalize(H * (2.0f * glm::dot(V, H)) - V);

                    float NoL = saturate(glm::dot(N, L));
                    if (NoL > 0.0f)
                    {
                        float NoH = saturate(glm::dot(N, H));
                        float VoH = saturate(glm::dot(V, H));

                        float D   = distributionGGX(NoH, roughness);
                        float pdf = (D * NoH / (4.0f * VoH + 0.0001f)) + 0.0001f;

                        float sampleSolidAngle = 1.0f / (static_cast<float>(sampleCount) * pdf + 0.0001f);
                        float sourceMip = 0.5f * std::log2(sampleSolidAngle / sourceTexelSolidAngle) + 1.0f;
                        sourceMip = std::max(sourceMip, 0.0f);

                        glm::vec3 sampleColor(sampleCubemapLod(environmentWithMips, L, sourceMip));

                        float luma = glm::dot(sampleColor, glm::vec3(0.2126f, 0.7152f, 0.0722f));
                        float maxLuma = 15.0f;
                        if (luma > maxLuma)
                        {
                            sampleColor *= (maxLuma / luma);
                        }

                        prefiltered += sampleColor * NoL;
                        totalWeight += NoL;
                    }
                }

                if (totalWeight > 0.0f)
                {
                    prefiltered /= totalWeight;
                }

                mip.faces[faceIndex][texelIndex] = glm::vec4(prefiltered, 1.0f);
            });
        }

        return result;
    }

    // ============================================================
    // Full Pipeline Structs
    // ============================================================

    struct GeneratedIbl
    {
        Cubemap skybox;
        Cubemap irradiance;
        Cubemap radiance;
    };

    struct IblGenerationSettings
    {
        uint32_t skyboxSize = 512;
        uint32_t irradianceSize = 32;
        uint32_t radianceSize = 256;

        uint32_t irradianceSamples = 1024;
        uint32_t radianceSamples = 1024;
    };

    inline GeneratedIbl generateIblFromEquirectangularHdr(
        Image2D const& hdr,
        IblGenerationSettings const& settings = {})
    {
        GeneratedIbl result{};
        result.skybox = equirectangularToCubemap(hdr, settings.skyboxSize);

        Cubemap skyboxWithMips = generateCubemapMipChain(result.skybox);

        result.irradiance = generateIrradianceCubemap(
            skyboxWithMips,
            settings.irradianceSize,
            settings.irradianceSamples);

        result.radiance = generatePrefilteredGGXCubemap(
            skyboxWithMips,
            settings.radianceSize,
            0,
            settings.radianceSamples);

        return result;
    }
}
