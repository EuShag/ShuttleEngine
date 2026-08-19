//
// Created by Shagu on 04.08.2026.
//

#ifndef SHUTTLEENGINE_COMMON_HPP
#define SHUTTLEENGINE_COMMON_HPP

#include "IncludeVulkan.hpp"

namespace shuttle::engine::render {
    struct AttachmentState {
        vk::ImageLayout layout;
        vk::AccessFlags2 accessFlags;
        vk::PipelineStageFlags2 stageFlags;
    };

    struct BufferState {
        vk::AccessFlags2 accessFlags;
        vk::PipelineStageFlags2 stageFlags;
    };
}

#endif //SHUTTLEENGINE_COMMON_HPP
