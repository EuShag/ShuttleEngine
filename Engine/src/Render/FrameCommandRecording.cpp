//
// Created by Shagu on 30.07.2026.
//

#include "Render.hpp"

#include <Assets/Formats/Geometry.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>


namespace shuttle::engine::render
{
    namespace {

        constexpr bool DebugClampIndirectDraws = false;

        uint32_t divRoundUp(
            uint32_t value,
            uint32_t divisor)
        {
            return (value + divisor - 1u) / divisor;
        }

        uint32_t makeCascadeViewMask(
            uint32_t cascadeCount)
        {
            cascadeCount =
                std::min(
                    cascadeCount,
                    MaxShadowCascades);

            return (1u << cascadeCount) - 1u;
        }

        void globalMemoryBarrier(
            vk::CommandBuffer cmd,
            vk::AccessFlags2 srcAccess,
            vk::AccessFlags2 dstAccess,
            vk::PipelineStageFlags2 srcStage,
            vk::PipelineStageFlags2 dstStage)
        {
            vk::MemoryBarrier2 barrier{
                .srcStageMask = srcStage,
                .srcAccessMask = srcAccess,
                .dstStageMask = dstStage,
                .dstAccessMask = dstAccess
            };

            vk::DependencyInfo dependencyInfo{
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barrier
            };

            cmd.pipelineBarrier2(
                dependencyInfo);
        }

        void computeToComputeBarrier(
            vk::CommandBuffer cmd)
        {
            globalMemoryBarrier(
                cmd,
                vk::AccessFlagBits2::eShaderWrite,
                vk::AccessFlagBits2::eShaderRead |
                    vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::PipelineStageFlagBits2::eComputeShader);
        }

        void computeToGraphicsBarrier(
            vk::CommandBuffer cmd)
        {
            globalMemoryBarrier(
                cmd,
                vk::AccessFlagBits2::eShaderWrite,
                vk::AccessFlagBits2::eShaderRead |
                    vk::AccessFlagBits2::eIndirectCommandRead |
                    vk::AccessFlagBits2::eIndexRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::PipelineStageFlagBits2::eDrawIndirect |
                    vk::PipelineStageFlagBits2::eVertexInput |
                    vk::PipelineStageFlagBits2::eVertexShader |
                    vk::PipelineStageFlagBits2::eFragmentShader);
        }

        void transferToComputeBarrier(
            vk::CommandBuffer cmd)
        {
            globalMemoryBarrier(
                cmd,
                vk::AccessFlagBits2::eTransferWrite,
                vk::AccessFlagBits2::eShaderRead |
                    vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::PipelineStageFlagBits2::eComputeShader);
        }

        void clearToComputeOrClearBarrier(vk::CommandBuffer cmd)
        {
            globalMemoryBarrier(
                cmd,

                // Previous vkCmdFillBuffer write.
                vk::AccessFlagBits2::eTransferWrite,

                // Next users:
                // - compute shader reads/writes
                // - another vkCmdFillBuffer write
                vk::AccessFlagBits2::eShaderRead |
                    vk::AccessFlagBits2::eShaderWrite |
                    vk::AccessFlagBits2::eTransferWrite,

                // vkCmdFillBuffer is treated as CLEAR by sync validation.
                vk::PipelineStageFlagBits2::eClear,

                vk::PipelineStageFlagBits2::eComputeShader |
                    vk::PipelineStageFlagBits2::eClear);
        }

        void resetHiZCounters(vk::CommandBuffer cmd, const DeviceFrameResources& frame)
        {
            cmd.fillBuffer(
                *frame.hizCountersBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            clearToComputeOrClearBarrier(cmd);
        }

        void graphicsDepthToComputeBarrier(
            vk::CommandBuffer const cmd)
        {
            globalMemoryBarrier(
                cmd,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::AccessFlagBits2::eShaderRead,
                vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::PipelineStageFlagBits2::eComputeShader);
        }

        void computeToFragmentBarrier(
            vk::CommandBuffer const cmd)
        {
            globalMemoryBarrier(
                cmd,
                vk::AccessFlagBits2::eShaderWrite,
                vk::AccessFlagBits2::eShaderRead,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::PipelineStageFlagBits2::eFragmentShader);
        }

        void transitionImage(
            vk::CommandBuffer cmd,
            vk::Image image,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags2 srcAccess,
            vk::AccessFlags2 dstAccess,
            vk::PipelineStageFlags2 srcStage,
            vk::PipelineStageFlags2 dstStage,
            vk::ImageAspectFlags aspectMask,
            uint32_t baseMipLevel,
            uint32_t levelCount,
            uint32_t baseArrayLayer,
            uint32_t layerCount)
        {
            vk::ImageMemoryBarrier2 barrier{
                .srcStageMask = srcStage,
                .srcAccessMask = srcAccess,
                .dstStageMask = dstStage,
                .dstAccessMask = dstAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = image,
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = aspectMask,
                    .baseMipLevel = baseMipLevel,
                    .levelCount = levelCount,
                    .baseArrayLayer = baseArrayLayer,
                    .layerCount = layerCount
                }
            };

            vk::DependencyInfo dependencyInfo{
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier
            };

            cmd.pipelineBarrier2(
                dependencyInfo);
        }

        void transitionDepthImage(
            vk::CommandBuffer cmd,
            vk::Image image,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags2 srcAccess,
            vk::AccessFlags2 dstAccess,
            vk::PipelineStageFlags2 srcStage,
            vk::PipelineStageFlags2 dstStage,
            uint32_t mipCount = 1,
            uint32_t layerCount = 1)
        {
            transitionImage(
                cmd,
                image,
                oldLayout,
                newLayout,
                srcAccess,
                dstAccess,
                srcStage,
                dstStage,
                vk::ImageAspectFlagBits::eDepth,
                0,
                mipCount,
                0,
                layerCount);
        }

        void transitionColorImage(
            vk::CommandBuffer cmd,
            vk::Image image,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags2 srcAccess,
            vk::AccessFlags2 dstAccess,
            vk::PipelineStageFlags2 srcStage,
            vk::PipelineStageFlags2 dstStage,
            uint32_t mipCount = 1,
            uint32_t layerCount = 1)
        {
            transitionImage(
                cmd,
                image,
                oldLayout,
                newLayout,
                srcAccess,
                dstAccess,
                srcStage,
                dstStage,
                vk::ImageAspectFlagBits::eColor,
                0,
                mipCount,
                0,
                layerCount);
        }

        void resetDrawList(
            vk::CommandBuffer cmd,
            const DeviceDrawListResources& drawList)
        {
            cmd.fillBuffer(
                *drawList.indirectCommandsBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            cmd.fillBuffer(
                *drawList.drawCountBuffer,
                0,
                sizeof(uint32_t),
                0);

            cmd.fillBuffer(
                *drawList.meshRangesBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            cmd.fillBuffer(
                *drawList.meshWriteCountersBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            cmd.fillBuffer(
                *drawList.instanceRemapBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            transferToComputeBarrier(cmd);
        }

        void resetFrameCounters(
    vk::CommandBuffer cmd,
    const DeviceFrameResources& frame)
        {
            cmd.fillBuffer(
                *frame.candidateCountBuffer,
                0,
                sizeof(uint32_t),
                0);

            cmd.fillBuffer(
                *frame.visibleCandidateCountBuffer,
                0,
                sizeof(uint32_t),
                0);

            cmd.fillBuffer(
                *frame.renderStatisticsBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            cmd.fillBuffer(
                *frame.visibilityMasksBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            cmd.fillBuffer(
                *frame.visibilityFlagsBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            cmd.fillBuffer(
                *frame.chosenMeshIdsBuffer,
                0,
                VK_WHOLE_SIZE,
                0);

            transferToComputeBarrier(cmd);
        }

        void bindAllDescriptorSets(
            vk::CommandBuffer cmd,
            vk::PipelineBindPoint bindPoint,
            const DeviceRendererResources& renderer,
            const DeviceEnvironmentResources& environment,
            const DeviceSceneResources& scene,
            vk::DescriptorSet frameSet)
        {
            std::array descriptorSets{
                renderer.rendererSet,
                environment.environmentSet,
                scene.sceneSet,
                frameSet
            };

            cmd.bindDescriptorSets(
                bindPoint,
                *renderer.pipelineLayout,
                0,
                descriptorSets,
                {});
        }

        void pushConstants(
            vk::CommandBuffer cmd,
            vk::PipelineLayout layout,
            vk::ShaderStageFlags,
            const void* data,
            uint32_t size)
        {
            constexpr vk::ShaderStageFlags allStages =
                vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eCompute;

            cmd.pushConstants(
                layout,
                allStages,
                0,
                size,
                data);
        }

        void dispatch1D(
            vk::CommandBuffer cmd,
            uint32_t itemCount,
            uint32_t localSize)
        {
            cmd.dispatch(
                std::max(
                    1u,
                    divRoundUp(
                        itemCount,
                        localSize)),
                1,
                1);
        }

        void dispatch2D(
            vk::CommandBuffer cmd,
            uint32_t width,
            uint32_t height,
            uint32_t localX,
            uint32_t localY)
        {
            cmd.dispatch(
                std::max(
                    1u,
                    divRoundUp(
                        width,
                        localX)),
                std::max(
                    1u,
                    divRoundUp(
                        height,
                        localY)),
                1);
        }

        void recordIndexedIndirectDraw(
            vk::CommandBuffer cmd,
            const DeviceSceneResources& scene,
            const DeviceDrawListResources& drawList,
            uint32_t commandCount)
        {
            cmd.bindIndexBuffer(
                *scene.indexBuffer,
                0,
                vk::IndexType::eUint32);

            const uint32_t maxDrawCount =
                DebugClampIndirectDraws
                    ? 1u
                    : commandCount;

            cmd.drawIndexedIndirectCount(
                *drawList.indirectCommandsBuffer,
                0,
                *drawList.drawCountBuffer,
                0,
                maxDrawCount,
                sizeof(vk::DrawIndexedIndirectCommand));
        }

    }

    vk::Result recordFrameCommands(
        vk::CommandBuffer cmd,

        const DeviceRendererResources& renderer,
        const DeviceSceneResources& scene,
        const DeviceEnvironmentResources& environment,
        DeviceFrameResources& frame,

        const HostSceneData& hostScene,
        const SceneFrameRequirements& requirements,

        const RenderTargets& renderTarget,

        const CascadeSetupPushConstants& cascadePushConstants,
        const GtaoPushConstants& gtaoPushConstants,
        const GtaoDenoisePushConstants& gtaoDenoisePushConstants,

        vk::ImageLayout swapchainOldLayout,

        const std::function<void(vk::CommandBuffer)>& recordUiCallback)
    {
        using assets::formats::geometry::MaxMeshLods;

        const uint32_t drawableCount =
            requirements.drawableObjectCount;

        const uint32_t meshCount =
            requirements.meshCount;

        const uint32_t commandCount =
            meshCount * MaxMeshLods;

        const uint32_t hiZMipCount =
            std::max(
                1u,
                static_cast<uint32_t>(
                    frame.hizMipViews.size()));

        const uint32_t shadowLayerCount =
            std::max(
                1u,
                std::min(
                    cascadePushConstants.cascadeCount,
                    MaxShadowCascades));

        const uint32_t shadowViewMask =
            makeCascadeViewMask(
                shadowLayerCount);

        const uint32_t renderWidth =
            renderTarget.renderTargetExtent.width;

        const uint32_t renderHeight =
            renderTarget.renderTargetExtent.height;

        struct PrefixScanPushConstants
        {
            uint32_t commandCount{};
        };


        PrefixScanPushConstants prefixScanPushConstants{
            .commandCount =
                commandCount
        };

        resetFrameCounters(
            cmd,
            frame);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.mainFrameSet);

        // ============================================================
        // 1. Scene Update
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.sceneUpdatePipeline);

        for (const auto& level : hostScene.levels)
        {
            if (level.nodeCount == 0)
            {
                continue;
            }

            struct SceneUpdatePushConstants
            {
                uint32_t startNodeIndex{};
                uint32_t nodeCount{};
            };

            SceneUpdatePushConstants pc{
                .startNodeIndex =
                    level.startNodeIndex,

                .nodeCount =
                    level.nodeCount
            };

            pushConstants(
                cmd,
                *renderer.pipelineLayout,
                vk::ShaderStageFlagBits::eCompute,
                &pc,
                sizeof(pc));

            dispatch1D(
                cmd,
                level.nodeCount,
                256);

            computeToComputeBarrier(
                cmd);
        }

        // ============================================================
        // 2. Frustum Cull
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.universalFrustumCullPipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 3. Cascade Setup
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.cascadeSetupPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &cascadePushConstants,
            sizeof(cascadePushConstants));

        cmd.dispatch(
            1,
            1,
            1);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 4. Occluder Resolve
        // ============================================================

        resetDrawList(
            cmd,
            frame.occluderDrawList);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.occluderFrameSet);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.occluderResolvePipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 5. Prefix Scan for Occluder Pass
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.prefixScanPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &prefixScanPushConstants,
            sizeof(prefixScanPushConstants));

        cmd.dispatch(
            1,
            1,
            1);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 6. Instance Resolve for Occluder Pass
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.instanceResolvePipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToGraphicsBarrier(
            cmd);

        // ============================================================
        // 7. Occluder Depth Pass
        // ============================================================

        transitionDepthImage(
            cmd,
            *frame.depthImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                vk::PipelineStageFlagBits2::eLateFragmentTests);

        {
            vk::RenderingAttachmentInfo depthAttachment{
                .imageView =
                    *frame.depthImageView,

                .imageLayout =
                    vk::ImageLayout::eDepthAttachmentOptimal,

                .loadOp =
                    vk::AttachmentLoadOp::eClear,

                .storeOp =
                    vk::AttachmentStoreOp::eStore,

                .clearValue =
                    vk::ClearValue{
                        .depthStencil =
                            vk::ClearDepthStencilValue{
                                .depth = 1.0f,
                                .stencil = 0
                            }
                    }
            };

            vk::RenderingInfo renderingInfo{
                .renderArea =
                    vk::Rect2D{
                        .offset = {0, 0},
                        .extent = renderTarget.renderTargetExtent
                    },

                .layerCount =
                    1,

                .colorAttachmentCount =
                    0,

                .pColorAttachments =
                    nullptr,

                .pDepthAttachment =
                    &depthAttachment
            };

            cmd.beginRendering(
                renderingInfo);

            cmd.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                *renderer.occluderPipeline);


            cmd.setViewport(
                0,
                vk::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(renderWidth),
                    .height = static_cast<float>(renderHeight),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                });

            cmd.setScissor(
                0,
                vk::Rect2D{
                    .offset = {0, 0},
                    .extent = renderTarget.renderTargetExtent
                });

            bindAllDescriptorSets(
                cmd,
                vk::PipelineBindPoint::eGraphics,
                renderer,
                environment,
                scene,
                frame.occluderFrameSet);

            recordIndexedIndirectDraw(
                cmd,
                scene,
                frame.occluderDrawList,
                commandCount);

            cmd.endRendering();
        }

        graphicsDepthToComputeBarrier(
            cmd);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.mainFrameSet);

        // ============================================================
        // 8. Linear Depth #1
        // ============================================================

        transitionDepthImage(
            cmd,
            *frame.depthImage,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eComputeShader);

        transitionColorImage(
            cmd,
            *frame.linearDepthImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral,
            {},
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eComputeShader);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.linearDepthPipeline);

        dispatch2D(
            cmd,
            renderWidth,
            renderHeight,
            8,
            8);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 9. Hi-Z Build #1
        // ============================================================

        transitionColorImage(
            cmd,
            *frame.linearDepthImage,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eShaderWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader);

        transitionColorImage(
            cmd,
            *frame.hizImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral,
            {},
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eComputeShader,
            hiZMipCount);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.hizBuildPipeline);

        struct HiZBuildPushConstants
        {
            uint32_t totalMipCount;
            uint32_t counterCount;
        };

        auto calculateHiZCounterCount = [](
            uint32_t width,
            uint32_t height,
            uint32_t mipCount)
        {
            uint32_t counterCount = 0;

            uint32_t mipWidth = width;
            uint32_t mipHeight = height;

            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                if (mip >= 5)
                {
                    counterCount += mipWidth * mipHeight;
                }

                mipWidth = std::max(1u, mipWidth / 2u);
                mipHeight = std::max(1u, mipHeight / 2u);
            }

            return counterCount;
        };

        const uint32_t hiZCounterCount =
            calculateHiZCounterCount(
                renderWidth,
                renderHeight,
                hiZMipCount);

        const vk::DeviceSize hiZCounterBufferSize =
            sizeof(uint32_t) * hiZCounterCount;

        HiZBuildPushConstants hiZPushConstants{
            .totalMipCount = hiZMipCount,
            .counterCount = hiZCounterCount
        };

        resetHiZCounters(cmd, frame);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.hizBuildPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &hiZPushConstants,
            sizeof(hiZPushConstants));

        dispatch2D(
            cmd,
            renderWidth,
            renderHeight,
            16,
            16);

        computeToComputeBarrier(
            cmd);

        transitionColorImage(
            cmd,
            *frame.hizImage,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eShaderWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader,
            hiZMipCount);

        // ============================================================
        // 10. Occlusion Cull Pass #1
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.occlusionCullPass1Pipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 11. Visible Depth Resolve
        // ============================================================

        resetDrawList(
            cmd,
            frame.visibleDepthDrawList);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.visibleDepthFrameSet);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.visibleDepthResolvePipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 12. Prefix Scan for Visible Depth
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.prefixScanPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &prefixScanPushConstants,
            sizeof(prefixScanPushConstants));

        cmd.dispatch(
            1,
            1,
            1);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 13. Instance Resolve for Visible Depth
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.instanceResolvePipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToGraphicsBarrier(
            cmd);

        // ============================================================
        // 14. Visible Depth Pass
        // ============================================================

        transitionDepthImage(
            cmd,
            *frame.depthImage,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::AccessFlagBits2::eShaderRead,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite |
                vk::AccessFlagBits2::eDepthStencilAttachmentRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                vk::PipelineStageFlagBits2::eLateFragmentTests);

        {
            vk::RenderingAttachmentInfo depthAttachment{
                .imageView =
                    *frame.depthImageView,

                .imageLayout =
                    vk::ImageLayout::eDepthAttachmentOptimal,

                .loadOp =
                    vk::AttachmentLoadOp::eLoad,

                .storeOp =
                    vk::AttachmentStoreOp::eStore
            };

            vk::RenderingInfo renderingInfo{
                .renderArea =
                    vk::Rect2D{
                        .offset = {0, 0},
                        .extent = renderTarget.renderTargetExtent
                    },

                .layerCount =
                    1,

                .colorAttachmentCount =
                    0,

                .pColorAttachments =
                    nullptr,

                .pDepthAttachment =
                    &depthAttachment
            };

            cmd.beginRendering(
                renderingInfo);

            cmd.setViewport(
                0,
                vk::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(renderWidth),
                    .height = static_cast<float>(renderHeight),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                });

            cmd.setScissor(
                0,
                vk::Rect2D{
                    .offset = {0, 0},
                    .extent = renderTarget.renderTargetExtent
                });

            bindAllDescriptorSets(
                cmd,
                vk::PipelineBindPoint::eGraphics,
                renderer,
                environment,
                scene,
                frame.visibleDepthFrameSet);

            cmd.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                *renderer.visibleDepthPipeline);

            recordIndexedIndirectDraw(
                cmd,
                scene,
                frame.visibleDepthDrawList,
                commandCount);

            cmd.endRendering();
        }

        graphicsDepthToComputeBarrier(
            cmd);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.mainFrameSet);

        // ============================================================
        // 15. Linear Depth #2
        // ============================================================

        transitionDepthImage(
            cmd,
            *frame.depthImage,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eComputeShader);

        transitionColorImage(
            cmd,
            *frame.linearDepthImage,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eGeneral,
            vk::AccessFlagBits2::eShaderRead,
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.linearDepthPipeline);

        dispatch2D(
            cmd,
            renderWidth,
            renderHeight,
            8,
            8);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 16. Hi-Z Build #2
        // ============================================================

        transitionColorImage(
            cmd,
            *frame.linearDepthImage,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eShaderWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader);

        transitionColorImage(
            cmd,
            *frame.hizImage,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eGeneral,
            vk::AccessFlagBits2::eShaderRead,
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader,
            hiZMipCount);

        resetHiZCounters(cmd, frame);
        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.hizBuildPipeline);

        pushConstants(
           cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &hiZPushConstants,
            sizeof(hiZPushConstants));

        dispatch2D(
            cmd,
            renderWidth,
            renderHeight,
            16,
            16);

        computeToComputeBarrier(
            cmd);

        transitionColorImage(
            cmd,
            *frame.hizImage,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eShaderWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader,
            hiZMipCount);

        // ============================================================
        // 17. Final Occlusion Cull Pass #2
        // ============================================================

        resetDrawList(
            cmd,
            frame.mainDrawList);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.mainFrameSet);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.occlusionCullPass2Pipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 18. Prefix Scan for Final Draw
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.prefixScanPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &prefixScanPushConstants,
            sizeof(prefixScanPushConstants));

        cmd.dispatch(
            1,
            1,
            1);

        computeToComputeBarrier(
            cmd);

        // ============================================================
        // 19. Instance Resolve for Final Draw
        // ============================================================

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.instanceResolvePipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToGraphicsBarrier(
            cmd);

        // ============================================================
        // 19.5 Shadow DrawList Build
        // ============================================================

        resetDrawList(
            cmd,
            frame.shadowDrawList);

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.shadowFrameSet);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.occlusionCullPass2Pipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToComputeBarrier(
            cmd);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.prefixScanPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &prefixScanPushConstants,
            sizeof(prefixScanPushConstants));

        cmd.dispatch(
            1,
            1,
            1);

        computeToComputeBarrier(
            cmd);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.instanceResolvePipeline);

        dispatch1D(
            cmd,
            drawableCount,
            64);

        computeToGraphicsBarrier(
            cmd);

        // ============================================================
        // 20. Shadow Pass
        // ============================================================

        transitionDepthImage(
            cmd,
            *frame.shadowMapImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                vk::PipelineStageFlagBits2::eLateFragmentTests,
            1,
            shadowLayerCount);

        {
            vk::RenderingAttachmentInfo depthAttachment{
                .imageView =
                    *frame.shadowMapImageView,

                .imageLayout =
                    vk::ImageLayout::eDepthAttachmentOptimal,

                .loadOp =
                    vk::AttachmentLoadOp::eClear,

                .storeOp =
                    vk::AttachmentStoreOp::eStore,

                .clearValue =
                    vk::ClearValue{
                        .depthStencil =
                            vk::ClearDepthStencilValue{
                                .depth = 1.0f,
                                .stencil = 0
                            }
                    }
            };

            vk::RenderingInfo renderingInfo{
                .renderArea =
                    vk::Rect2D{
                        .offset = {0, 0},
                        .extent =
                            vk::Extent2D{
                                cascadePushConstants.shadowMapResolution,
                                cascadePushConstants.shadowMapResolution
                            }
                    },

                .layerCount =
                    1,

                .viewMask =
                    shadowViewMask,

                .colorAttachmentCount =
                    0,

                .pColorAttachments =
                    nullptr,

                .pDepthAttachment =
                    &depthAttachment
            };

            cmd.beginRendering(
                renderingInfo);

            cmd.setViewport(
                0,
                vk::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width =
                        static_cast<float>(
                            cascadePushConstants.shadowMapResolution),
                    .height =
                        static_cast<float>(
                            cascadePushConstants.shadowMapResolution),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                });

            cmd.setScissor(
                0,
                vk::Rect2D{
                    .offset = {0, 0},
                    .extent =
                        vk::Extent2D{
                            cascadePushConstants.shadowMapResolution,
                            cascadePushConstants.shadowMapResolution
                        }
                });

            bindAllDescriptorSets(
                cmd,
                vk::PipelineBindPoint::eGraphics,
                renderer,
                environment,
                scene,
                frame.shadowFrameSet);

            cmd.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                *renderer.shadowPipeline);

            cmd.setDepthBias(
                1.25f,
                0.0f,
                1.75f);

            recordIndexedIndirectDraw(
                cmd,
                scene,
                frame.shadowDrawList,
                commandCount);

            cmd.endRendering();
        }

        transitionDepthImage(
            cmd,
            *frame.shadowMapImage,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eFragmentShader,
            1,
            shadowLayerCount);

        // ============================================================
        // 21. GTAO
        // ============================================================

        bindAllDescriptorSets(
            cmd,
            vk::PipelineBindPoint::eCompute,
            renderer,
            environment,
            scene,
            frame.mainFrameSet);

        transitionColorImage(
            cmd,
            *frame.gtaoImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral,
            {},
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eComputeShader);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.gtaoPipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &gtaoPushConstants,
            sizeof(gtaoPushConstants));

        dispatch2D(
            cmd,
            renderWidth,
            renderHeight,
            8,
            8);

        // ============================================================
        // 22. GTAO Denoise
        // ============================================================

        transitionColorImage(
            cmd,
            *frame.gtaoImage,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eShaderWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eComputeShader);

        transitionColorImage(
            cmd,
            *frame.gtaoFilteredImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral,
            {},
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eComputeShader);

        cmd.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            *renderer.gtaoDenoisePipeline);

        pushConstants(
            cmd,
            *renderer.pipelineLayout,
            vk::ShaderStageFlagBits::eCompute,
            &gtaoDenoisePushConstants,
            sizeof(gtaoDenoisePushConstants));

        dispatch2D(
            cmd,
            renderWidth,
            renderHeight,
            8,
            8);

        transitionColorImage(
            cmd,
            *frame.gtaoFilteredImage,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eShaderWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eFragmentShader);

        // ============================================================
        // 23. Main Pass + Skybox
        // ============================================================

        transitionColorImage(
            cmd,
            renderTarget.colorAttachmentImage,

            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,

            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,

            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput);

        transitionDepthImage(
            cmd,
            *frame.depthImage,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::AccessFlagBits2::eShaderRead,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                vk::PipelineStageFlagBits2::eLateFragmentTests);

        {
            vk::RenderingAttachmentInfo colorAttachment{
                .imageView =
                    *renderTarget.colorAttachmentImageView,

                .imageLayout =
                    vk::ImageLayout::eColorAttachmentOptimal,

                .loadOp =
                    vk::AttachmentLoadOp::eClear,

                .storeOp =
                    vk::AttachmentStoreOp::eStore,

                .clearValue =
                    vk::ClearValue{
                        .color =
                            vk::ClearColorValue{
                                .float32 =
                                    std::array<float, 4>{
                                        0.02f,
                                        0.02f,
                                        0.025f,
                                        1.0f
                                    }
                            }
                    }
            };

            vk::RenderingAttachmentInfo depthAttachment{
                .imageView =
                    *frame.depthImageView,

                .imageLayout =
                    vk::ImageLayout::eDepthAttachmentOptimal,

                .loadOp =
                    vk::AttachmentLoadOp::eLoad,

                .storeOp =
                    vk::AttachmentStoreOp::eStore
            };

            vk::RenderingInfo renderingInfo{
                .renderArea =
                    vk::Rect2D{
                        .offset = {0, 0},
                        .extent = renderTarget.renderTargetExtent
                    },

                .layerCount =
                    1,

                .colorAttachmentCount =
                    1,

                .pColorAttachments =
                    &colorAttachment,

                .pDepthAttachment =
                    &depthAttachment
            };

            cmd.beginRendering(
                renderingInfo);

            cmd.setViewport(
                0,
                vk::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(renderWidth),
                    .height = static_cast<float>(renderHeight),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                });

            cmd.setScissor(
                0,
                vk::Rect2D{
                    .offset = {0, 0},
                    .extent = renderTarget.renderTargetExtent
                });

            bindAllDescriptorSets(
                cmd,
                vk::PipelineBindPoint::eGraphics,
                renderer,
                environment,
                scene,
                frame.mainFrameSet);

            cmd.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                *renderer.mainPipeline);

            recordIndexedIndirectDraw(
                cmd,
                scene,
                frame.mainDrawList,
                commandCount);

            cmd.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                *renderer.skyboxPipeline);

            cmd.draw(
                36,
                1,
                0,
                0);



            /*if (recordUiCallback)
            {
                recordUiCallback(
                    cmd);
            }*/

            cmd.endRendering();
        }

        transitionColorImage(
            cmd,
            renderTarget.colorAttachmentImage,

            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,

            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},

            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe);

        return vk::Result::eSuccess;
    }
}
