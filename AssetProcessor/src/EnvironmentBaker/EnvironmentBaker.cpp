#include "EnvironmentBaker.hpp"

#include <cmft/cubemapfilter.h>
#include <cmft/allocator.h>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <thread>

#include <glm/gtc/packing.hpp>
#include <vector>
#include <cmath>


namespace shuttle_engine::assets
{

    static glm::vec3 decodeRGBE(const uint8_t* rgbe)
    {
        const uint8_t r = rgbe[0];
        const uint8_t g = rgbe[1];
        const uint8_t b = rgbe[2];
        const uint8_t e = rgbe[3];

        if (e == 0)
        {
            return glm::vec3(0.0f);
        }

        const float scale = std::ldexp(1.0f, int(e) - 128);

        return glm::vec3(
            float(r) / 255.0f,
            float(g) / 255.0f,
            float(b) / 255.0f
        ) * scale;
    }

    static EnvironmentTextureData convertCmftRgbeToRgba16f(
        const cmft::Image& image)
    {
        EnvironmentTextureData result{};

        result.width = image.m_width;
        result.height = image.m_height;
        result.mipCount = image.m_numMips;
        result.faceCount = image.m_numFaces;
        result.format = static_cast<uint32_t>(
            vk::Format::eR16G16B16A16Sfloat
        );

        const uint32_t texelCount =
            image.m_dataSize / 4; // RGBE = 4 bytes per texel

        result.data.resize(
            size_t(texelCount) * 4 * sizeof(uint16_t)
        );

        const auto* src =
            static_cast<const uint8_t*>(image.m_data);

        auto* dst =
            reinterpret_cast<uint16_t*>(result.data.data());

        for (uint32_t i = 0; i < texelCount; ++i)
        {
            glm::vec3 rgb = decodeRGBE(src + i * 4);

            dst[i * 4 + 0] = glm::packHalf1x16(rgb.r);
            dst[i * 4 + 1] = glm::packHalf1x16(rgb.g);
            dst[i * 4 + 2] = glm::packHalf1x16(rgb.b);
            dst[i * 4 + 3] = glm::packHalf1x16(1.0f);
        }

        return result;
    }


    EnvironmentBakeResult EnvironmentBaker::bake(
        const std::filesystem::path& hdrFile)
    {
        EnvironmentBakeResult result{};

        cmft::Image hdrImage{};
        cmft::Image skybox{};
        cmft::Image irradiance{};
        cmft::Image radiance{};

        bool ok = cmft::imageLoad(
            hdrImage,
            hdrFile.string().c_str(),
            cmft::TextureFormat::Null,
            allocator()
        );

        if (!ok)
        {
            throw std::runtime_error(
                "Failed to load HDR file."
            );
        }

        std::cout << "[CMFT] HDR loaded\n";

        ok = cmft::imageCubemapFromLatLong(
            skybox,
            hdrImage,
            true,
            allocator()
        );

        if (!ok)
        {
            throw std::runtime_error(
                "Failed to generate cubemap."
            );
        }

        std::cout << "[CMFT] Cubemap generated\n";

        ok = cmft::imageIrradianceFilterSh(
            irradiance,
            32,
            skybox,
            allocator()
        );

        if (!ok)
        {
            throw std::runtime_error(
                "Failed to generate irradiance map."
            );
        }

        std::cout << "[CMFT] Irradiance generated\n";

        uint8_t threadCount =
            static_cast<uint8_t>(
                std::max(
                    1u,
                    std::thread::hardware_concurrency()
                )
            );

        ok = cmft::imageRadianceFilter(
            radiance,
            128,                              // DEBUG PRESET
            cmft::LightingModel::BlinnBrdf,
            false,
            8,
            10,
            0,
            skybox,
            cmft::EdgeFixup::Warp,
            threadCount,
            nullptr,
            allocator()
        );

        if (!ok)
        {
            throw std::runtime_error(
                "Failed to generate radiance map."
            );
        }

        std::cout << "[CMFT] Radiance generated\n";

        cmft::imageUnload(
            hdrImage,
            allocator()
        );


        std::cout
            << "Skybox bytes 0:     "
            << skybox.m_dataSize
            << '\n';

        std::cout
            << "Irradiance bytes 0: "
            << irradiance.m_dataSize
            << '\n';

        std::cout
            << "Radiance bytes 0:   "
            << radiance.m_dataSize
            << '\n';


        std::cout << "[] Converting images" << std::endl;

        result.skybox =
            convertCmftRgbeToRgba16f(skybox);

        result.irradiance =
            convertCmftRgbeToRgba16f(irradiance);

        result.radiance =
            convertCmftRgbeToRgba16f(radiance);


        std::cout
            << "Skybox bytes:     "
            << result.skybox.data.size()
            << '\n';

        std::cout
            << "Irradiance bytes: "
            << result.irradiance.data.size()
            << '\n';

        std::cout
            << "Radiance bytes:   "
            << result.radiance.data.size()
            << '\n';

        return result;
    }
}