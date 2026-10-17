//
// Created by Shagu on 13.09.2026.
//

#ifndef SHUTTLEENGINE_VIEWPORTRESOURCES_HPP
#define SHUTTLEENGINE_VIEWPORTRESOURCES_HPP
#include <vector>
#include "vulkan_helper_functions/vulkan_helper_functions.hpp"

namespace shuttle::engine {
    struct ViewportResources {
        vk::Extent2D extent;

        // Viewport Attachments
        std::vector<AttachmentOutput> m_depthAttachmentOutputs{};
        std::vector<AttachmentOutput> m_colorAttachmentOutputs{};
        std::vector<AttachmentOutput> m_debugOutputs1{};
        std::vector<AttachmentOutput> m_debugOutputs2{};
        std::vector<AttachmentOutput> m_debugOutputs3{};
        std::vector<AttachmentOutput> m_debugOutputs4{};
    };

    struct ViewportIndices {
        uint64_t releaseTextureIndex;
        uint64_t debug1TextureIndex;
        uint64_t debug2TextureIndex;
        uint64_t debug3TextureIndex;
        uint64_t debug4TextureIndex;
    };
}

#endif //SHUTTLEENGINE_VIEWPORTRESOURCES_HPP
