//
// Created by Shagu on 25.05.2026.
//
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Render.hpp"

#include <iostream>
#include <ostream>
#include <utility>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"

namespace shuttle_engine {

    // =========================================================================
    // PbrRender::create  (unchanged)
    // =========================================================================
    vk::ResultValue<PbrRender> PbrRender::create(vk::Device device, vk::ImageLayout finalLayout) {
        PbrRender render;

        if (auto res = render.initMainRenderPass(device, finalLayout); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initShadowRenderPass(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initPbrMaterialSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initSceneDataSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initSamplerDescriptorSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initModelDataSetLayout(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initSamplers(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initSamplerDescriptorSet(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initMainPipelineLayout(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initShadowPipelineLayout(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initMainPipeline(device); res != vk::Result::eSuccess)
            return {res, {}};
        if (auto res = render.initShadowPipeline(device); res != vk::Result::eSuccess)
            return {res, {}};

        return {vk::Result::eSuccess, std::move(render)};
    }

    // =========================================================================
    // uploadScene  —  BlobSceneData → GPU resources
    // =========================================================================
    vk::ResultValue<DeviceSceneData> PbrRender::uploadScene(
        const BlobSceneData& blob,
        vk::Queue transferQueue,
        vk::Device device,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator)
    {
        DeviceSceneData resultData;

        // ------------------------------------------------------------------ //
        // 1. Prepare mesh/draw data from the blob scene graph.
        // ------------------------------------------------------------------ //
        MeshData meshData = prepareMeshData(blob);
        if (meshData.positionData.empty()) return {vk::Result::eSuccess, {}};

        const vk::DeviceSize posSize      = meshData.positionData.size();
        const vk::DeviceSize attrSize     = meshData.attributeData.size();
        const vk::DeviceSize idxSize      = meshData.indexData.size();
        const vk::DeviceSize cmdSize      = meshData.indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand);
        const vk::DeviceSize modelSize    = meshData.modelDatas.size() * sizeof(ModelData);
        const vk::DeviceSize geomRequired = alignUp(posSize + attrSize + idxSize + cmdSize + modelSize, vk::DeviceSize{256});

        // ------------------------------------------------------------------ //
        // 2. Pre-compute texture layout inside the staging buffer.
        // ------------------------------------------------------------------ //
        const uint32_t texCount = static_cast<uint32_t>(blob.textures.size());
        std::vector<vk::DeviceSize> texStagingOffsets(texCount, 0);
        vk::DeviceSize totalTexSize = 0;
        constexpr vk::DeviceSize texAlignment = 16;
        for (uint32_t i = 0; i < texCount; ++i) {
            totalTexSize = alignUp(totalTexSize, texAlignment);
            texStagingOffsets[i] = totalTexSize;
            uint64_t sz = BlobSceneData::calcTextureDataSize(blob.textures[i]);
            totalTexSize += alignUp(static_cast<vk::DeviceSize>(sz), texAlignment);
        }

        // Fallback pixel data: 5 × 4 bytes
        constexpr uint32_t kFallbackCount = 5;
        constexpr vk::DeviceSize kFallbackTexBytes = kFallbackCount * 4;
        static constexpr std::array<std::array<uint8_t,4>, kFallbackCount> kFallbackPixels{{
            {255, 255, 255, 255}, // albedo  — white
            {128, 128, 255, 255}, // normal  — neutral blue
            {255, 255,   0, 255}, // orm     — AO=1, Roughness=1, Metallic=0
            {  0,   0,   0, 255}, // emissive— black
            {255, 255, 255, 255}  // height  — white
        }};
        static constexpr std::array<vk::Format, kFallbackCount> kFallbackFormats{{
            vk::Format::eR8G8B8A8Srgb,   // albedo
            vk::Format::eR8G8B8A8Unorm,  // normal
            vk::Format::eR8G8B8A8Unorm,  // orm
            vk::Format::eR8G8B8A8Srgb,   // emissive
            vk::Format::eR8G8B8A8Unorm,  // height
        }};

        // Per-material UBO layout
        const uint32_t matCount = static_cast<uint32_t>(blob.materials.size());
        const vk::DeviceSize uboAligned = alignUp(vk::DeviceSize{sizeof(HostMaterialProperties)}, vk::DeviceSize{256});
        const vk::DeviceSize totalUboSize = matCount * uboAligned;

        // ------------------------------------------------------------------ //
        // 3. Allocate one global staging buffer.
        // ------------------------------------------------------------------ //
        vk::DeviceSize stagingOffset = 0;

        // Block layout inside staging:
        //   [geometry (pos+attr+idx+cmd+model)] [textures] [fallback pixels] [material UBOs]
        vk::DeviceSize texBlockOff      = geomRequired;
        vk::DeviceSize fallbackBlockOff = alignUp(texBlockOff + totalTexSize, texAlignment);
        vk::DeviceSize uboBlockOff      = alignUp(fallbackBlockOff + kFallbackTexBytes, vk::DeviceSize{256});
        vk::DeviceSize totalStaging     = uboBlockOff + totalUboSize;

        auto [stgRes, stagingBuffer] = allocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = totalStaging,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .sharingMode = vk::SharingMode::eExclusive
            }, resources::MemoryUsage::eCpuOnly);
        if (stgRes != vk::Result::eSuccess) return {stgRes, {}};

        // ------------------------------------------------------------------ //
        // 4. Write geometry into staging.
        // ------------------------------------------------------------------ //
        auto [stgMeshRes, stagingMeshInfo] = prepareStagingBufferMeshData(
            meshData, allocator, *stagingBuffer, stagingOffset);
        if (stgMeshRes != vk::Result::eSuccess) return {stgMeshRes, {}};

        // ------------------------------------------------------------------ //
        // 5. Write texture data into staging.
        // ------------------------------------------------------------------ //
        for (uint32_t i = 0; i < texCount; ++i) {
            auto span = blob.getTextureData(i);
            if (auto r = allocator.writeBufferFromHost({
                .dstBuffer = *stagingBuffer,
                .dstBufferOffset = texBlockOff + texStagingOffsets[i],
                .srcData = span.data(),
                .dataSize = span.size()
            }); r != vk::Result::eSuccess) return {r, {}};
        }

        // ------------------------------------------------------------------ //
        // 6. Write fallback pixel data into staging.
        // ------------------------------------------------------------------ //
        for (uint32_t i = 0; i < kFallbackCount; ++i) {
            if (auto r = allocator.writeBufferFromHost({
                .dstBuffer = *stagingBuffer,
                .dstBufferOffset = fallbackBlockOff + i * 4,
                .srcData = kFallbackPixels[i].data(),
                .dataSize = 4
            }); r != vk::Result::eSuccess) return {r, {}};
        }

        // ------------------------------------------------------------------ //
        // 7. Write per-material UBOs into staging.
        // ------------------------------------------------------------------ //
        for (uint32_t i = 0; i < matCount; ++i) {
            const auto& mat = blob.materials[i];
            HostMaterialProperties props{};
            props.baseColorFactor  = mat.baseColorFactor;
            props.metallicFactor   = mat.metallicFactor;
            props.roughnessFactor  = mat.roughnessFactor;
            props.occlusionStrength= mat.occlusionStrength;
            props.emissiveStrength = mat.emissiveStrength;
            props.emissiveFactor   = glm::vec3(mat.emissiveFactor);
            if (auto r = allocator.writeBufferFromHost({
                .dstBuffer = *stagingBuffer,
                .dstBufferOffset = uboBlockOff + i * uboAligned,
                .srcData = &props,
                .dataSize = sizeof(HostMaterialProperties)
            }); r != vk::Result::eSuccess) return {r, {}};
        }

        // ------------------------------------------------------------------ //
        // 8. Create device geometry buffers.
        // ------------------------------------------------------------------ //
        auto [devMeshRes, deviceMesh] = prepareDeviceMeshData(stagingMeshInfo, allocator);
        if (devMeshRes != vk::Result::eSuccess) return {devMeshRes, {}};

        resultData.vertexBuffer                    = std::move(deviceMesh.vertexBuffer);
        resultData.positionAttributeOffset         = deviceMesh.positionAttributeOffset;
        resultData.normalUvTangentAttributeOffset  = deviceMesh.normalUvTangentAttributeOffset;
        resultData.indexBuffer                     = std::move(deviceMesh.indexBuffer);
        resultData.indexBufferOffset               = deviceMesh.indexBufferOffset;
        resultData.indirectDrawBuffer              = std::move(deviceMesh.indirectBuffer);
        resultData.indirectDrawBufferOffset        = deviceMesh.indirectBufferOffset;
        resultData.indirectDraws                   = std::move(deviceMesh.indirectDraws);
        resultData.modelSsbo                       = std::move(deviceMesh.modelSsboBuffer);

        // ------------------------------------------------------------------ //
        // 9. Create device texture images for all blob textures.
        // ------------------------------------------------------------------ //
        resultData.textureImages.resize(texCount);
        resultData.textureViews.resize(texCount);

        for (uint32_t i = 0; i < texCount; ++i) {
            const auto& meta = blob.textures[i];
            vk::ImageCreateInfo imgCI{
                .imageType   = vk::ImageType::e2D,
                .format      = static_cast<vk::Format>(meta.format),
                .extent      = vk::Extent3D{ meta.width, meta.height, 1 },
                .mipLevels   = meta.mipCount,
                .arrayLayers = meta.isCubemap ? 6u : 1u,
                .samples     = vk::SampleCountFlagBits::e1,
                .tiling      = vk::ImageTiling::eOptimal,
                .usage       = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                .initialLayout = vk::ImageLayout::eUndefined
            };
            auto [ir, img] = allocator.createAndAllocateImageUnique(imgCI, resources::MemoryUsage::eGpuOnly);
            if (ir != vk::Result::eSuccess) return {ir, {}};
            resultData.textureImages[i] = std::move(img);

            auto [vr, view] = device.createImageViewUnique(vk::ImageViewCreateInfo{
                .image    = *resultData.textureImages[i],
                .viewType = meta.isCubemap ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
                .format   = static_cast<vk::Format>(meta.format),
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, meta.mipCount, 0, meta.isCubemap ? 6u : 1u }
            });
            if (vr != vk::Result::eSuccess) return {vr, {}};
            resultData.textureViews[i] = std::move(view);
        }

        // ------------------------------------------------------------------ //
        // 10. Create fallback 1x1 images.
        // ------------------------------------------------------------------ //
        for (uint32_t i = 0; i < kFallbackCount; ++i) {
            auto [ir, img] = allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D, .format = kFallbackFormats[i],
                    .extent = {1, 1, 1}, .mipLevels = 1, .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1, .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                    .initialLayout = vk::ImageLayout::eUndefined
                }, resources::MemoryUsage::eGpuOnly);
            if (ir != vk::Result::eSuccess) return {ir, {}};
            resultData.fallbackImages[i] = std::move(img);

            auto [vr, view] = device.createImageViewUnique(vk::ImageViewCreateInfo{
                .image = *resultData.fallbackImages[i], .viewType = vk::ImageViewType::e2D,
                .format = kFallbackFormats[i],
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
            });
            if (vr != vk::Result::eSuccess) return {vr, {}};
            resultData.fallbackViews[i] = std::move(view);
        }

        // ------------------------------------------------------------------ //
        // 11. Create per-material UBO device buffers.
        // ------------------------------------------------------------------ //
        std::vector<resources::UniqueAllocatedBuffer> matUbos(matCount);
        for (uint32_t i = 0; i < matCount; ++i) {
            auto [br, buf] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(HostMaterialProperties),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                    .sharingMode = vk::SharingMode::eExclusive
                }, resources::MemoryUsage::eGpuOnly);
            if (br != vk::Result::eSuccess) return {br, {}};
            matUbos[i] = std::move(buf);
        }

        // ------------------------------------------------------------------ //
        // 12. Create descriptor pool and allocate sets.
        // ------------------------------------------------------------------ //
        uint32_t totalSets = matCount + 1; // +1 for model SSBO
        std::array<vk::DescriptorPoolSize, 3> poolSizes{{
            { vk::DescriptorType::eUniformBuffer, std::max(1u, matCount) },
            { vk::DescriptorType::eStorageBuffer, 1 },
            { vk::DescriptorType::eSampledImage,  std::max(1u, matCount * 5) }
        }};

        auto [poolRes, descPool] = device.createDescriptorPoolUnique({
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = totalSets,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        });
        if (poolRes != vk::Result::eSuccess) return {poolRes, {}};

        // Material descriptor sets (set 3 = pbrMaterialSetLayout)
        std::vector<vk::DescriptorSetLayout> matLayouts(matCount, *pbrMaterialSetLayout);
        vk::ResultValue<std::vector<vk::DescriptorSet>> matSetsRes{{}, {}};
        if (matCount > 0) {
            matSetsRes = device.allocateDescriptorSets({
                .descriptorPool = *descPool,
                .descriptorSetCount = matCount,
                .pSetLayouts = matLayouts.data()
            });
            if (matSetsRes.result != vk::Result::eSuccess) return {matSetsRes.result, {}};
        }

        // Model SSBO descriptor set (set 1)
        auto [modelSetRes, modelSets] = device.allocateDescriptorSets({
            .descriptorPool = *descPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &*modelSetLayout
        });
        if (modelSetRes != vk::Result::eSuccess) return {modelSetRes, {}};
        resultData.modelSsboDescriptorSet = modelSets[0];

        vk::DescriptorBufferInfo ssboInfo{ .buffer = *resultData.modelSsbo, .offset = 0, .range = vk::WholeSize };
        device.updateDescriptorSets(
            { vk::WriteDescriptorSet{
                .dstSet = resultData.modelSsboDescriptorSet, .dstBinding = 0,
                .descriptorCount = 1, .descriptorType = vk::DescriptorType::eStorageBuffer,
                .pBufferInfo = &ssboInfo
            }}, {});

        // ------------------------------------------------------------------ //
        // 13. Record and submit all GPU copies.
        // ------------------------------------------------------------------ //
        auto [allocCmdRes, tempCmds] = device.allocateCommandBuffersUnique({
            .commandPool = transferCommandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1
        });
        if (allocCmdRes != vk::Result::eSuccess) return {allocCmdRes, {}};

        vk::CommandBuffer cmd = tempCmds[0].get();
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        // Copy geometry
        stagingMeshInfo.recordCopyCommandsToBuffer(
            cmd,
            *resultData.vertexBuffer,
            *resultData.indexBuffer,
            *resultData.indirectDrawBuffer,
            *resultData.modelSsbo);

        // Helper: transition image layout
        auto transitionImage = [&](vk::Image img, uint32_t mips, uint32_t layers,
                                   vk::ImageLayout oldL, vk::ImageLayout newL,
                                   vk::AccessFlags srcA, vk::AccessFlags dstA,
                                   vk::PipelineStageFlags srcS, vk::PipelineStageFlags dstS)
        {
            vk::ImageMemoryBarrier b{
                .srcAccessMask = srcA, .dstAccessMask = dstA,
                .oldLayout = oldL, .newLayout = newL,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = img,
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, mips, 0, layers }
            };
            cmd.pipelineBarrier(srcS, dstS, {}, nullptr, nullptr, b);
        };

        // Copy blob textures
        for (uint32_t i = 0; i < texCount; ++i) {
            const auto& meta = blob.textures[i];
            uint32_t layers = meta.isCubemap ? 6u : 1u;

            transitionImage(*resultData.textureImages[i], meta.mipCount, layers,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                {}, vk::AccessFlagBits::eTransferWrite,
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

            std::vector<vk::BufferImageCopy> regions;
            vk::DeviceSize mipOff = 0;
            for (uint32_t mip = 0; mip < meta.mipCount; ++mip) {
                uint32_t w = std::max(1u, meta.width  >> mip);
                uint32_t h = std::max(1u, meta.height >> mip);
                uint32_t mipBytes = BlobSceneData::calcMipSize(w, h, meta.format);
                regions.push_back({
                    .bufferOffset      = texBlockOff + texStagingOffsets[i] + mipOff,
                    .imageSubresource  = { vk::ImageAspectFlagBits::eColor, mip, 0, layers },
                    .imageExtent       = { w, h, 1 }
                });
                mipOff += mipBytes;
            }
            cmd.copyBufferToImage(*stagingBuffer, *resultData.textureImages[i],
                                  vk::ImageLayout::eTransferDstOptimal, regions);

            transitionImage(*resultData.textureImages[i], meta.mipCount, layers,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::AccessFlagBits::eTransferWrite, {},
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe);
        }

        // Copy fallback textures
        for (uint32_t i = 0; i < kFallbackCount; ++i) {
            transitionImage(*resultData.fallbackImages[i], 1, 1,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                {}, vk::AccessFlagBits::eTransferWrite,
                vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer);

            vk::BufferImageCopy region{
                .bufferOffset = fallbackBlockOff + i * 4,
                .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
                .imageExtent = { 1, 1, 1 }
            };
            cmd.copyBufferToImage(*stagingBuffer, *resultData.fallbackImages[i],
                                  vk::ImageLayout::eTransferDstOptimal, region);

            transitionImage(*resultData.fallbackImages[i], 1, 1,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::AccessFlagBits::eTransferWrite, {},
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe);
        }

        // Copy material UBOs
        for (uint32_t i = 0; i < matCount; ++i) {
            cmd.copyBuffer(*stagingBuffer, *matUbos[i], {
                vk::BufferCopy{ .srcOffset = uboBlockOff + i * uboAligned, .dstOffset = 0, .size = sizeof(HostMaterialProperties) }
            });
        }

        cmd.end();

        vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &cmd };
        if (auto r = transferQueue.submit(1, &submitInfo, nullptr); r != vk::Result::eSuccess)
            return {r, {}};
        transferQueue.waitIdle();

        // ------------------------------------------------------------------ //
        // 14. Fill material descriptor sets using global texture views.
        // ------------------------------------------------------------------ //
        resultData.materials.reserve(matCount);
        auto getView = [&](int32_t idx, uint32_t fallbackIdx) -> vk::ImageView {
            if (idx >= 0 && static_cast<uint32_t>(idx) < texCount)
                return *resultData.textureViews[idx];
            return *resultData.fallbackViews[fallbackIdx];
        };

        for (uint32_t i = 0; i < matCount; ++i) {
            const auto& mat = blob.materials[i];
            std::array<vk::ImageView, 5> views{
                getView(mat.albedoTexIdx,   0),
                getView(mat.normalTexIdx,   1),
                getView(mat.ormTexIdx,      2),
                getView(mat.emissiveTexIdx, 3),
                getView(mat.heightTexIdx,   4),
            };
            fillDescriptorSet(device, matSetsRes.value[i], *matUbos[i], views);

            resultData.materials.push_back(DeviceSceneData::RenderMaterialData{
                .deviceMaterialInfo = { std::move(matUbos[i]) },
                .materialSet        = matSetsRes.value[i]
            });
        }

        resultData.descriptorPool = std::move(descPool);

        // ------------------------------------------------------------------ //
        // 15. Build scene lighting from blob.
        // ------------------------------------------------------------------ //
        resultData.sceneLightingData = SceneLightingData{
            .ambient              = glm::vec4(20.0f, 50.0f, 70.0f, 0.10f),
            .directionalLightCount= static_cast<uint32_t>(blob.dirLights.size()),
            .pointLightCount      = static_cast<uint32_t>(blob.pointLights.size()),
            .spotLightCount       = static_cast<uint32_t>(blob.spotLights.size()),
        };

        for (const auto& dl : blob.dirLights) {
            glm::vec4 direction{};
            if (dl.directionAndIntensity == glm::vec4(0.0f, -1.0f, 0.0f, 1.0f)) {
                direction = glm::vec4{glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)), 0.0f};
            }
            else {
                direction = glm::vec4(dl.directionAndIntensity.x, dl.directionAndIntensity.y, dl.directionAndIntensity.z, dl.directionAndIntensity.w);
            }
            resultData.directionalLightDatas.push_back({
                .direction        = direction,
                .color            = glm::vec4(dl.color, dl.directionAndIntensity.w),
                .lightSpaceMatrix = glm::mat4(1.0f)
            });
        }
        if (resultData.directionalLightDatas.empty()) {
            resultData.directionalLightDatas.push_back({
                .direction        = glm::vec4{glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f)), 0.0f},
                .color            = glm::vec4(1.0f, 0.95f, 0.9f, 1.0f),
                .lightSpaceMatrix = glm::mat4(1.0f)
            });
            resultData.sceneLightingData.directionalLightCount = 1;
        }

        return {vk::Result::eSuccess, std::move(resultData)};
    }
    void PbrRender::recordRenderFrameCommands(
        DeviceSceneData const& sceneData,
        vk::CommandBuffer cmd,
        FrameData const& frameData,
        RenderTargets const& targets,
        std::function<void(vk::CommandBuffer)> const& additionalCommands) const
    {
        // =========================================================================
        // ПАСС 1: РЕНДЕРИНГ В КАРТУ ТЕНЕЙ (SHADOW PASS)
        // =========================================================================
        {
            vk::ClearValue shadowClear{ .depthStencil = { .depth = 1.0f, .stencil = 0 } };

            vk::RenderPassBeginInfo shadowPassBegin{
                .renderPass = *shadowRenderPass,
                .framebuffer = *frameData.shadowRenderPassFramebuffer, // Берем актуальный FB из таргетов
                .renderArea = vk::Rect2D{ {0, 0}, frameData.shadowExtent },
                .clearValueCount = 1,
                .pClearValues = &shadowClear
            };

            cmd.beginRenderPass(shadowPassBegin, vk::SubpassContents::eInline);

            vk::Viewport viewport{
                .x = 0.0f, .y = 0.0f,
                .width = static_cast<float>(frameData.shadowExtent.width),
                .height = static_cast<float>(frameData.shadowExtent.height),
                .minDepth = 0.0f, .maxDepth = 1.0f
            };
            vk::Rect2D scissor{
                .offset = {0, 0},
                .extent = frameData.shadowExtent
            };

            cmd.setViewport(0, 1, &viewport);
            cmd.setScissor(0, 1, &scissor);
            cmd.setDepthBias(0.0f, 0.0f, 0.0f);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *shadowPipeline);

            std::array shadowDescriptorSets{
                sceneData.modelSsboDescriptorSet,
                frameData.sceneDataSet
            };

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *shadowPipelineLayout,
                0, shadowDescriptorSets,
                {}
            );

            uint32_t totalIndirectCount = 0;
            for (auto indirectDraw : sceneData.indirectDraws) {
                totalIndirectCount += indirectDraw.commandCount;
            }

            vk::Buffer vBuffer = *sceneData.vertexBuffer;
            vk::DeviceSize offset = sceneData.positionAttributeOffset;
            cmd.bindVertexBuffers(0, 1, &vBuffer, &offset);
            cmd.bindIndexBuffer(*sceneData.indexBuffer, sceneData.indexBufferOffset, vk::IndexType::eUint32);

            cmd.drawIndexedIndirect(
                *sceneData.indirectDrawBuffer,
                sceneData.indirectDrawBufferOffset,
                totalIndirectCount,
                sizeof(vk::DrawIndexedIndirectCommand)
            );

            cmd.endRenderPass();
        }

        // =========================================================================
        // ПАСС 2: ОСНОВНОЙ РЕНДЕР (MAIN PASS)
        // =========================================================================
        {
            std::array clearValues{
                vk::ClearValue{ .color = vk::ClearColorValue{.float32 = std::array{sceneData.sceneLightingData.ambient.r, sceneData.sceneLightingData.ambient.g, sceneData.sceneLightingData.ambient.b, 1.0f} } },
                vk::ClearValue{ .depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 } }
            };

            vk::RenderPassBeginInfo mainPassBegin{
                .renderPass = *mainRenderPass,
                .framebuffer = *targets.mainRenderPassFramebuffer, // Берем FB по индексу кадра
                .renderArea = vk::Rect2D{ {0, 0}, targets.renderTargetExtent },
                .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                .pClearValues = clearValues.data()
            };

            cmd.beginRenderPass(mainPassBegin, vk::SubpassContents::eInline);

            vk::Viewport viewport{
                .x = 0.0f, .y = 0.0f,
                .width = static_cast<float>(targets.renderTargetExtent.width),
                .height = static_cast<float>(targets.renderTargetExtent.height),
                .minDepth = 0.0f, .maxDepth = 1.0f
            };
            vk::Rect2D scissor{
                .offset = {0, 0},
                .extent = targets.renderTargetExtent
            };

            cmd.setViewport(0, 1, &viewport);
            cmd.setScissor(0, 1, &scissor);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mainPipeline);

            std::array mainRenderingSets {
                samplersSet,     // Будет доступен как set = 0
                sceneData.modelSsboDescriptorSet,        // Будет доступен как set = 1
                frameData.sceneDataSet // Будет доступен как set = 2
            };

            // Биндим Global Scene Set
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *mainPipelineLayout,
                0, mainRenderingSets,
                {}
            );

            std::array<vk::Buffer, 2> vBuffers{ *sceneData.vertexBuffer, *sceneData.vertexBuffer };
            std::array vOffsets{ sceneData.positionAttributeOffset, sceneData.normalUvTangentAttributeOffset };
            cmd.bindVertexBuffers(0, 2, vBuffers.data(), vOffsets.data());
            cmd.bindIndexBuffer(*sceneData.indexBuffer, sceneData.indexBufferOffset, vk::IndexType::eUint32);

            // Рисуем все материалы
            for (const auto& indirectDraw : sceneData.indirectDraws) {
                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    *mainPipelineLayout,
                    3, 1, &sceneData.materials[indirectDraw.materialIndex].materialSet,
                    0, nullptr
                );

                cmd.drawIndexedIndirect(
                    *sceneData.indirectDrawBuffer,
                    indirectDraw.indirectBufferOffset,
                    indirectDraw.commandCount,
                    sizeof(vk::DrawIndexedIndirectCommand)
                );
            }

            additionalCommands(cmd);

            cmd.endRenderPass();
        }
    }

    static glm::mat4 calculateLightSpaceMatrix(
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::vec4& lightDir,
        float shadowMapResolution) // Передайте сюда размер карты теней, например, 2048.0f
    {
        // Шаг 1: Получаем вершины Frustum камеры в мировых координатах
        glm::mat4 inv = glm::inverse(projMatrix * viewMatrix);
        std::vector<glm::vec4> frustumCorners;
        for (unsigned int x = 0; x < 2; ++x) {
            for (unsigned int y = 0; y < 2; ++y) {
                for (unsigned int z = 0; z < 2; ++z) {
                    glm::vec4 pt = inv * glm::vec4(
                        2.0f * static_cast<float>(x) - 1.0f,
                        2.0f * static_cast<float>(y) - 1.0f,
                        static_cast<float>(z),
                        1.0f
                    );
                    frustumCorners.push_back(pt / pt.w);
                }
            }
        }

        // Шаг 2: Находим геометрический центр Frustum
        glm::vec3 center{0.0f};
        for (const auto& worldCorner : frustumCorners) {
            center += glm::vec3(worldCorner);
        }
        center /= static_cast<float>(frustumCorners.size());

        // СТАБИЛИЗАЦИЯ ШАГ 1: Считаем радиус ограничивающей сферы Frustum.
        // Вместо динамического AABB мы берем максимальное расстояние от центра до угла frustum.
        // Это делает размеры проекции света константными при вращении камеры.
        float sphereRadius = 0.0f;
        for (const auto& corner : frustumCorners) {
            float dist = glm::length(glm::vec3(corner) - center);
            sphereRadius = std::max(sphereRadius, dist);
        }
        // Округляем радиус с небольшим запасом
        sphereRadius = std::ceil(sphereRadius * 1.1f);

        // Отодвигаем источник света далеко назад на основе фиксированного радиуса
        glm::vec3 normalizedLightDir = glm::normalize(lightDir);
        glm::vec3 lightPos = center - (normalizedLightDir * sphereRadius * 2.0f);

        glm::mat4 lightView = glm::lookAt(
            lightPos,
            center,
            glm::vec3{0.0f, 1.0f, 0.0f}
        );

        // Изначальные жесткие границы на основе сферы (они симметричны и неизменны)
        float minX = -sphereRadius;
        float maxX =  sphereRadius;
        float minY = -sphereRadius;
        float maxY =  sphereRadius;

        // СТАБИЛИЗАЦИЯ ШАГ 2: Привязка к пиксельной сетке (Texel Snapping).
        // Находим, сколько мировых единиц приходится на один пиксель текстуры тени
        float worldTexelSize = (maxX - minX) / shadowMapResolution;

        // Переводим центр в пространство света, чтобы округлить его координаты
        glm::vec4 lightSpaceCenter = lightView * glm::vec4(center, 1.0f);

        // Округляем координаты до ближайшего текселя
        lightSpaceCenter.x = std::floor(lightSpaceCenter.x / worldTexelSize) * worldTexelSize;
        lightSpaceCenter.y = std::floor(lightSpaceCenter.y / worldTexelSize) * worldTexelSize;

        // Восстанавливаем скорректированную матрицу lightView с учетом округленного центра
        // Это убирает микро-дрожание при плавном перемещении камеры
        glm::vec3 snappedCenter = glm::vec3(glm::inverse(lightView) * lightSpaceCenter);
        lightView = glm::lookAt(snappedCenter - (normalizedLightDir * sphereRadius * 2.0f), snappedCenter, glm::vec3{0.0f, 1.0f, 0.0f});

        // Для Z-плоскостей оставляем надежный глубокий диапазон
        float minZ = -sphereRadius * 10.0f;
        float maxZ =  sphereRadius * 10.0f;

        // Строим итоговую ортографическую проекцию без ручных инверсий
        glm::mat4 lightProjection = glm::orthoLH_ZO(minX, maxX, minY, maxY, minZ, maxZ);

        return lightProjection * lightView;
    }

    // =========================================================================
    // НОВЫЙ МЕТОД: Умное обновление данных камеры и теней на GPU
    // =========================================================================
    vk::Result PbrRender::updateSceneData(
        resources::DeviceAllocator const& allocator,
        DeviceSceneData& sceneData,
        FrameData& frameData,
        glm::mat4 const& viewMatrix,
        glm::mat4 const& projectionMatrix,
        glm::mat4 const& shortProjectionMatrix,
        glm::vec3 const& cameraPos)
    {

        glm::mat4 lightSpaceMatrix = calculateLightSpaceMatrix(viewMatrix, shortProjectionMatrix, sceneData.directionalLightDatas[0].direction, 4096.0f);

        DirectionalLightData sunLightData{
            .direction = sceneData.directionalLightDatas[0].direction,
            .color = sceneData.directionalLightDatas[0].color,
            .lightSpaceMatrix = lightSpaceMatrix
        };

        sceneData.directionalLightDatas[0] = sunLightData;

        // 2. Обновляем UBO камеры (Set 0, Binding 0)
        CameraUniformData cameraData{
            .viewProj = projectionMatrix * viewMatrix,
            .cameraPos = cameraPos
        };

        auto writeSceneLightDataResult = allocator.writeBufferFromHost({
            .dstBuffer = *frameData.lightInfoUbo,
            .dstBufferOffset = 0,
            .srcData = &sceneData.sceneLightingData,
            .dataSize = sizeof(SceneLightingData)
        });

        if (writeSceneLightDataResult != vk::Result::eSuccess) return writeSceneLightDataResult;

        auto writeDirectionalLightDataResult = allocator.writeBufferFromHost({
            .dstBuffer = *frameData.lightSsbo,
            .dstBufferOffset = 0,
            .srcData = sceneData.directionalLightDatas.data(),
            .dataSize = sizeof(DirectionalLightData)
        });

        if (writeDirectionalLightDataResult != vk::Result::eSuccess) return writeDirectionalLightDataResult;

        auto writeCameraDataResult = allocator.writeBufferFromHost({
            .dstBuffer = *frameData.cameraUbo,
            .dstBufferOffset = 0,
            .srcData = &cameraData,
            .dataSize = sizeof(CameraUniformData)
        });

        if (writeCameraDataResult != vk::Result::eSuccess) return writeCameraDataResult;

        return vk::Result::eSuccess;
    }

    vk::ResultValue<std::vector<RenderTargets>> PbrRender::createRenderTargets(
        vk::Device device,
        resources::DeviceAllocator const &allocator,
        std::vector<vk::Image> const &targetImages,
        vk::Extent2D renderTargetExtent) const{

        std::vector<RenderTargets> result;
        result.reserve(targetImages.size());

        for (auto const& target : targetImages) {
            auto [depthBufferCreateResult, uniqueDepthBuffer] = allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .extent = vk::Extent3D{
                        .width = renderTargetExtent.width,
                        .height = renderTargetExtent.height,
                        .depth = 1
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eGpuOnly
            );

            if (depthBufferCreateResult != vk::Result::eSuccess) return {depthBufferCreateResult, {}};

            auto [colorImageViewCreateResult, uniqueColorImageView] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = target,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eB8G8R8A8Srgb,
                    .components = {
                        .r = vk::ComponentSwizzle::eIdentity,
                        .g = vk::ComponentSwizzle::eIdentity,
                        .b = vk::ComponentSwizzle::eIdentity,
                        .a = vk::ComponentSwizzle::eIdentity
                    },
                    .subresourceRange = vk::ImageSubresourceRange{
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );

            if (colorImageViewCreateResult != vk::Result::eSuccess) return {colorImageViewCreateResult, {}};

            auto [depthBufferImageViewCreateResult, uniqueDepthBufferImageView] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = *uniqueDepthBuffer,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .components = {
                        .r = vk::ComponentSwizzle::eIdentity,
                        .g = vk::ComponentSwizzle::eIdentity,
                        .b = vk::ComponentSwizzle::eIdentity,
                        .a = vk::ComponentSwizzle::eIdentity
                    },
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );

            if (depthBufferImageViewCreateResult != vk::Result::eSuccess) return {depthBufferImageViewCreateResult, {}};

            std::array attachments {
                *uniqueColorImageView,
                *uniqueDepthBufferImageView
            };

            auto [framebufferCreateResult, uniqueFramebuffer] = device.createFramebufferUnique(
                vk::FramebufferCreateInfo{
                    .renderPass = *mainRenderPass,
                    .attachmentCount = 2,
                    .pAttachments = attachments.data(),
                    .width = renderTargetExtent.width,
                    .height = renderTargetExtent.height,
                    .layers = 1
                }
            );

            if (framebufferCreateResult != vk::Result::eSuccess) return {framebufferCreateResult, {}};

            result.emplace_back(RenderTargets{
                .depthBufferImage = std::move(uniqueDepthBuffer),
                .depthBufferImageView = std::move(uniqueDepthBufferImageView),
                .colorAttachmentImageView = std::move(uniqueColorImageView),
                .mainRenderPassFramebuffer = std::move(uniqueFramebuffer),
                .renderTargetExtent = renderTargetExtent
            });
        }
        return {vk::Result::eSuccess, std::move(result)};
    }

    vk::ResultValue<std::vector<FrameData>> PbrRender::createFrameDatas(
        vk::Device device,
        resources::DeviceAllocator const &allocator,
        vk::Extent2D shadowMapExtent,
        vk::DescriptorPool descriptorPool,
        uint32_t frameCount
    ) const {
        std::vector<FrameData> result;
        result.reserve(frameCount);

        std::vector layouts{frameCount, *pbrSceneDataSetLayout};

        auto [createSceneSetsResult, uniqueSceneSets] = device.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo{
                .descriptorPool = descriptorPool,
                .descriptorSetCount = frameCount,
                .pSetLayouts = layouts.data()
            }
        );

        if (createSceneSetsResult != vk::Result::eSuccess) return {createSceneSetsResult, {}};

        for (uint32_t i = 0; i < frameCount; i++) {
            auto [shadowMapImageCreateResult, uniqueShadowMapImage] = allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .extent = {
                        .width = shadowMapExtent.width,
                        .height = shadowMapExtent.height,
                        .depth = 1
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eGpuOnly
            );

            if (shadowMapImageCreateResult != vk::Result::eSuccess) return {shadowMapImageCreateResult, {}};

            auto [shadowMapImageViewCreateResult, uniqueShadowMapImageView] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = *uniqueShadowMapImage,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .components = {
                        .r = vk::ComponentSwizzle::eIdentity,
                        .g = vk::ComponentSwizzle::eIdentity,
                        .b = vk::ComponentSwizzle::eIdentity,
                        .a = vk::ComponentSwizzle::eIdentity
                    },
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eDepth,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
                }
            );

            if (shadowMapImageViewCreateResult != vk::Result::eSuccess) return {shadowMapImageViewCreateResult, {}};

            std::array attachments {
                *uniqueShadowMapImageView
            };

            auto [framebufferCreateResult, uniqueFramebuffer] = device.createFramebufferUnique(
                vk::FramebufferCreateInfo{
                    .renderPass = *shadowRenderPass,
                    .attachmentCount = 1,
                    .pAttachments = attachments.data(),
                    .width = shadowMapExtent.width,
                    .height = shadowMapExtent.height,
                    .layers = 1
                }
            );

            if (framebufferCreateResult != vk::Result::eSuccess) return {framebufferCreateResult, {}};

            auto [lightSceneDataUboCreateResult, uniqueLightSceneDataUbo] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(SceneLightingData),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlags {
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped)
                }
            );

            if (lightSceneDataUboCreateResult != vk::Result::eSuccess) return {lightSceneDataUboCreateResult, {}};

            auto [lightSsboCreateResult, uniqueLightSsbo] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(DirectionalLightData),
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlags {
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped)
                }
            );

            if (lightSsboCreateResult != vk::Result::eSuccess) return {lightSsboCreateResult, {}};

            auto [cameraUboCreateResult, uniqueCameraUbo] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(CameraUniformData),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer
                },
                resources::MemoryUsage::eCpuToGpu,
                resources::AllocationCreateFlags {
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eHostAccessSequentialWrite) |
                    static_cast<uint32_t>(resources::AllocationCreateFlagBits::eMapped)
                }
            );

            if (cameraUboCreateResult != vk::Result::eSuccess) return {cameraUboCreateResult, {}};

            vk::DescriptorBufferInfo cameraUboDescriptorInfo {
                .buffer = *uniqueCameraUbo,
                .offset = 0,
                .range = sizeof(CameraUniformData)
            };

            vk::DescriptorBufferInfo sceneLightDataUboDescriptorInfo {
                .buffer = *uniqueLightSceneDataUbo,
                .offset = 0,
                .range = sizeof(SceneLightingData)
            };

            vk::DescriptorBufferInfo lightSsboDescriptorInfo {
                .buffer = *uniqueLightSsbo,
                .offset = 0,
                .range = sizeof(DirectionalLightData)
            };

            vk::DescriptorImageInfo shadowMapImageDescriptorInfo {
                .imageView = *uniqueShadowMapImageView,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
            };

            std::array sceneDataDescriptorWrites {
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &cameraUboDescriptorInfo
                },
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &sceneLightDataUboDescriptorInfo
                },
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &lightSsboDescriptorInfo
                },
                vk::WriteDescriptorSet {
                    .dstSet = uniqueSceneSets[i],
                    .dstBinding = 3,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eSampledImage,
                    .pImageInfo = &shadowMapImageDescriptorInfo
                }
            };

            device.updateDescriptorSets(
                sceneDataDescriptorWrites,
                {}
            );

            result.emplace_back(
                FrameData{
                    .shadowMapImage = std::move(uniqueShadowMapImage),
                    .shadowMapImageView = std::move(uniqueShadowMapImageView),
                    .shadowRenderPassFramebuffer = std::move(uniqueFramebuffer),
                    .shadowExtent = shadowMapExtent,
                    .cameraUbo = std::move(uniqueCameraUbo),
                    .lightInfoUbo = std::move(uniqueLightSceneDataUbo),
                    .lightSsbo = std::move(uniqueLightSsbo),
                    .sceneDataSet = std::move(uniqueSceneSets[i]),
                }
            );
        }
        return {vk::Result::eSuccess, std::move(result)};
    }
}
