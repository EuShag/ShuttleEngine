//
// Created by Shagu on 30.05.2026.
//
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_QUAT_DATA_WXYZ
#include "Render.hpp"
#include "VulkanHelperFunctions/VulkanHelperFunctions.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

namespace shuttle_engine {

    struct DrawInstance {
        glm::mat4 transform;
        uint32_t  materialIndex;
        uint32_t  meshIndex;
    };

    struct MeshDescription {
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t  vertexOffset;
    };

    // Build world transforms for every node (BFS order → parents always come before children).
    MeshData PbrRender::prepareMeshData(BlobSceneData const& blob) {
        if (blob.meshes.empty() || blob.sceneNodes.empty()) return {};

        // -----------------------------------------------------------------------
        // 1. Compute world transforms propagating from parent nodes.
        //    Nodes are in BFS (level) order, so parent index < child index.
        // -----------------------------------------------------------------------
        const size_t nodeCount = blob.sceneNodes.size();
        std::vector<glm::mat4> worldTransforms(nodeCount, glm::mat4(1.0f));

        for (size_t i = 0; i < nodeCount; ++i) {
            const auto& node = blob.sceneNodes[i];

            // Build local TRS matrix
            glm::mat4 T = glm::translate(glm::mat4(1.0f), node.localTranslation);
            // localRotationQuat is stored as (x,y,z,w) in glm::vec4
            glm::quat q(node.localRotationQuat.w,
                        node.localRotationQuat.x,
                        node.localRotationQuat.y,
                        node.localRotationQuat.z);
            glm::mat4 R = glm::mat4_cast(q);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), node.localScale);
            glm::mat4 local = T * R * S;

            if (node.parentIndex == format::INVALID_INDEX_U32) {
                worldTransforms[i] = local;
            } else {
                worldTransforms[i] = worldTransforms[node.parentIndex] * local;
            }
        }

        // -----------------------------------------------------------------------
        // 2. Collect draw instances for nodes that reference a mesh.
        // -----------------------------------------------------------------------
        std::vector<DrawInstance> drawInstances;
        drawInstances.reserve(nodeCount);

        for (size_t i = 0; i < nodeCount; ++i) {
            const auto& node = blob.sceneNodes[i];
            if (node.instanceIndex == format::INVALID_INDEX_U32) continue;

            uint32_t meshIdx = node.instanceIndex;
            if (meshIdx >= static_cast<uint32_t>(blob.meshes.size())) continue;

            uint32_t matIdx = static_cast<uint32_t>(blob.meshes[meshIdx].defaultMaterialIndex);
            if (static_cast<int32_t>(matIdx) < 0 || matIdx >= static_cast<uint32_t>(blob.materials.size()))
                matIdx = 0;

            drawInstances.push_back({ worldTransforms[i], matIdx, meshIdx });
        }

        if (drawInstances.empty()) return {};

        // -----------------------------------------------------------------------
        // 3. Sort by material then mesh for minimal descriptor set rebinds.
        // -----------------------------------------------------------------------
        std::ranges::sort(drawInstances, [](const DrawInstance& a, const DrawInstance& b) {
            if (a.materialIndex != b.materialIndex) return a.materialIndex < b.materialIndex;
            return a.meshIndex < b.meshIndex;
        });

        // -----------------------------------------------------------------------
        // 4. Copy raw geometry bytes from blob (entire merged buffers).
        //    All meshes share one contiguous position, attribute, and index buffer.
        // -----------------------------------------------------------------------
        MeshData result;

        if (!blob.meshes.empty()) {
            auto posSpan  = blob.getMeshPositionData(0);
            auto attrSpan = blob.getMeshAttributeData(0);
            auto idxSpan  = blob.getMeshIndexData(0);

            result.positionData.assign(posSpan.begin(),  posSpan.end());
            result.attributeData.assign(attrSpan.begin(), attrSpan.end());
            result.indexData.assign(idxSpan.begin(),     idxSpan.end());
        }

        // -----------------------------------------------------------------------
        // 5. Build per-mesh descriptions using LOD[0] data from blob.
        // -----------------------------------------------------------------------
        std::vector<MeshDescription> meshDescs;
        meshDescs.reserve(blob.meshes.size());
        for (const auto& mh : blob.meshes) {
            meshDescs.push_back({
                .indexCount   = mh.lods[0].indexCount,
                .firstIndex   = mh.lods[0].firstIndex,
                .vertexOffset = mh.lods[0].vertexOffset
            });
        }

        // -----------------------------------------------------------------------
        // 6. Build indirect draw commands and model matrices.
        // -----------------------------------------------------------------------
        result.indirectCommands.reserve(drawInstances.size());
        result.modelDatas.reserve(drawInstances.size());
        result.indirectDrawCalls.reserve(blob.materials.size());

        uint32_t currentMaterial   = drawInstances.front().materialIndex;
        uint32_t currentMesh       = drawInstances.front().meshIndex;
        uint32_t cmdCountThisMat   = 0;
        uint32_t firstCmdThisMat   = 0;
        uint32_t instanceIndexBase = 0;
        uint32_t instanceCount     = 0;

        auto flushMesh = [&]() {
            const auto& desc = meshDescs[currentMesh];
            result.indirectCommands.push_back({
                .indexCount    = desc.indexCount,
                .instanceCount = instanceCount,
                .firstIndex    = desc.firstIndex,
                .vertexOffset  = desc.vertexOffset,
                .firstInstance = instanceIndexBase
            });
            instanceIndexBase += instanceCount;
            instanceCount = 0;
            ++cmdCountThisMat;
        };

        auto flushMaterial = [&]() {
            result.indirectDrawCalls.push_back({
                .materialIndex       = currentMaterial,
                .commandCount        = cmdCountThisMat,
                .indirectBufferOffset = firstCmdThisMat * static_cast<uint32_t>(sizeof(vk::DrawIndexedIndirectCommand))
            });
            firstCmdThisMat += cmdCountThisMat;
            cmdCountThisMat = 0;
        };

        for (auto& di : drawInstances) {
            bool meshChanged     = (di.meshIndex     != currentMesh);
            bool materialChanged = (di.materialIndex != currentMaterial);

            if (meshChanged || materialChanged) {
                flushMesh();
                if (materialChanged) {
                    flushMaterial();
                    currentMaterial = di.materialIndex;
                }
                currentMesh = di.meshIndex;
            }

            result.modelDatas.push_back({
                .modelMatrix  = di.transform,
                .normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(di.transform))))
            });
            ++instanceCount;
        }

        // Flush the last mesh and material group.
        flushMesh();
        flushMaterial();

        return result;
    }

    // =========================================================================
    // Staging buffer preparation (unchanged API; updated for raw byte vectors)
    // =========================================================================

    vk::ResultValue<StagingBufferMeshData> PbrRender::prepareStagingBufferMeshData(
        MeshData const& meshData,
        resources::DeviceAllocator const& deviceAllocator,
        resources::AllocatedBuffer stagingBuffer,
        vk::DeviceSize& stagingBufferOffset
    ) {
        const vk::DeviceSize positionsSize = meshData.positionData.size();
        const vk::DeviceSize attribsSize   = meshData.attributeData.size();
        const vk::DeviceSize indicesSize   = meshData.indexData.size();
        const vk::DeviceSize commandsSize  = meshData.indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand);
        const vk::DeviceSize modelsSize    = meshData.modelDatas.size() * sizeof(ModelData);

        vk::DeviceSize positionAttributeOffset             = stagingBufferOffset;
        vk::DeviceSize normalUvTangentAttributeOffset      = positionAttributeOffset + positionsSize;
        vk::DeviceSize indicesOffset                       = normalUvTangentAttributeOffset + attribsSize;
        vk::DeviceSize indirectCommandsOffset              = indicesOffset + indicesSize;
        vk::DeviceSize modelDatasOffset                    = indirectCommandsOffset + commandsSize;

        stagingBufferOffset = alignUp(modelDatasOffset + modelsSize, vk::DeviceSize(256));

        if (auto r = deviceAllocator.writeBufferFromHost({ .dstBuffer = stagingBuffer, .dstBufferOffset = positionAttributeOffset,        .srcData = meshData.positionData.data(),           .dataSize = positionsSize }); r != vk::Result::eSuccess) return {r, {}};
        if (auto r = deviceAllocator.writeBufferFromHost({ .dstBuffer = stagingBuffer, .dstBufferOffset = normalUvTangentAttributeOffset,  .srcData = meshData.attributeData.data(),          .dataSize = attribsSize   }); r != vk::Result::eSuccess) return {r, {}};
        if (auto r = deviceAllocator.writeBufferFromHost({ .dstBuffer = stagingBuffer, .dstBufferOffset = indicesOffset,                   .srcData = meshData.indexData.data(),              .dataSize = indicesSize   }); r != vk::Result::eSuccess) return {r, {}};
        if (auto r = deviceAllocator.writeBufferFromHost({ .dstBuffer = stagingBuffer, .dstBufferOffset = indirectCommandsOffset,          .srcData = meshData.indirectCommands.data(),       .dataSize = commandsSize  }); r != vk::Result::eSuccess) return {r, {}};
        if (auto r = deviceAllocator.writeBufferFromHost({ .dstBuffer = stagingBuffer, .dstBufferOffset = modelDatasOffset,                .srcData = meshData.modelDatas.data(),             .dataSize = modelsSize    }); r != vk::Result::eSuccess) return {r, {}};

        return {
            vk::Result::eSuccess,
            {
                .stagingBuffer                                   = stagingBuffer,
                .vertexBufferSize                                = positionsSize + attribsSize,
                .positionAttributeVertexBindingBufferOffset      = positionAttributeOffset,
                .normalUvTangentAttributeVertexBindingBufferOffset = normalUvTangentAttributeOffset,
                .indexBufferSize                                 = indicesSize,
                .indicesBufferOffset                             = indicesOffset,
                .indirectCommandsBufferSize                      = commandsSize,
                .indirectCommandsBufferOffset                    = indirectCommandsOffset,
                .modelDatasBufferSize                            = modelsSize,
                .modelDatasBufferOffset                          = modelDatasOffset,
                .indirectDrawCalls                               = meshData.indirectDrawCalls
            }
        };
    }

    vk::Result PbrRender::initBuiltinResources(
        vk::Device device,
        vk::Queue transferQueue,
        vk::CommandPool transferCommandPool,
        resources::DeviceAllocator const& allocator)
    {
        const std::filesystem::path brdfPath =
            std::filesystem::current_path() /
            "Resources" /
            "Engine" /
            "brdf_lut.dds";

        std::cout << "[PbrRender] Loading BRDF LUT: "
                  << brdfPath.string()
                  << std::endl;

        tinyddsloader::DDSFile brdfFile;
        auto result = brdfFile.Load(R"(..\resources\engine\brdf_lut.dds)");

        if (result != tinyddsloader::Result::Success)
        {
            std::cerr << "[PbrRender] Failed to load BRDF LUT DDS."
                      << std::endl;
            return vk::Result::eErrorInitializationFailed;
        }

        const uint32_t width = brdfFile.GetWidth();
        const uint32_t height = brdfFile.GetHeight();
        const uint32_t mipCount = brdfFile.GetMipCount();

        const vk::Format format = vk::Format::eR16G16Sfloat;

        assert(
            brdfFile.GetFormat() ==
            tinyddsloader::DDSFile::DXGIFormat::R16G16_Float
        );


        const auto* imageData =
            brdfFile.GetImageData(0, 0);

        vk::DeviceSize offset = 0;

        for (uint32_t mip = 0; mip < brdfFile.GetMipCount(); ++mip)
        {
            for (uint32_t face = 0; face < brdfFile.GetArraySize(); ++face)
            {
                const auto* img =
                    brdfFile.GetImageData(mip, face);

                // region.bufferOffset = offset

                offset += img->m_memSlicePitch;
            }
        }

        const vk::DeviceSize dataSize = offset;

        auto [stgRes, stagingBuffer] =
            allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = dataSize,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc,
                    .sharingMode = vk::SharingMode::eExclusive
                },
                resources::MemoryUsage::eCpuOnly
            );

        if (stgRes != vk::Result::eSuccess)
            return stgRes;

        std::vector<std::byte> textureData;
        textureData.resize(dataSize);

        std::byte* dst = textureData.data();

        for (uint32_t mip = 0; mip < brdfFile.GetMipCount(); ++mip)
        {
            for (uint32_t array = 0; array < brdfFile.GetArraySize(); ++array)
            {
                const auto* img =
                    brdfFile.GetImageData(mip, array);

                std::memcpy(
                    dst,
                    img->m_mem,
                    img->m_memSlicePitch
                );

                dst += img->m_memSlicePitch;
            }
        }

        if (auto r = allocator.writeBufferFromHost({
            .dstBuffer = *stagingBuffer,
            .dstBufferOffset = 0,
            .srcData = textureData.data(),
            .dataSize = textureData.size(),
        }); r != vk::Result::eSuccess)
        {
            return r;
        }

        auto [imgRes, image] =
            allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .imageType = vk::ImageType::e2D,
                    .format = format,
                    .extent = vk::Extent3D{
                        width,
                        height,
                        1
                    },
                    .mipLevels = mipCount,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage =
                        vk::ImageUsageFlagBits::eTransferDst |
                        vk::ImageUsageFlagBits::eSampled,
                    .initialLayout = vk::ImageLayout::eUndefined
                },
                resources::MemoryUsage::eGpuOnly
            );

        if (imgRes != vk::Result::eSuccess)
            return imgRes;

        brdfLutImage = std::move(image);

        auto [viewRes, view] =
            device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = *brdfLutImage,
                    .viewType = vk::ImageViewType::e2D,
                    .format = format,
                    .subresourceRange = {
                        vk::ImageAspectFlagBits::eColor,
                        0,
                        mipCount,
                        0,
                        1
                    }
                }
            );

        if (viewRes != vk::Result::eSuccess)
            return viewRes;

        brdfLutImageView = std::move(view);

        auto [cmdRes, tempCmds] =
            device.allocateCommandBuffersUnique({
                .commandPool = transferCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1
            });

        if (cmdRes != vk::Result::eSuccess)
            return cmdRes;

        vk::CommandBuffer cmd = tempCmds[0].get();

        cmd.begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        auto transitionImage = [&](vk::Image img,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags srcAccess,
            vk::AccessFlags dstAccess,
            vk::PipelineStageFlags srcStage,
            vk::PipelineStageFlags dstStage)
        {
            vk::ImageMemoryBarrier barrier{
                .srcAccessMask = srcAccess,
                .dstAccessMask = dstAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = img,
                .subresourceRange = {
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    mipCount,
                    0,
                    1
                }
            };

            cmd.pipelineBarrier(
                srcStage,
                dstStage,
                {},
                nullptr,
                nullptr,
                barrier
            );
        };

        transitionImage(
            *brdfLutImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            {},
            vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer
        );

        std::vector<vk::BufferImageCopy> regions;

        offset = 0;

        for (uint32_t mip = 0; mip < mipCount; ++mip)
        {
            const uint32_t mipWidth =
                std::max(1u, width >> mip);

            const uint32_t mipHeight =
                std::max(1u, height >> mip);

            const vk::DeviceSize mipSize =
                static_cast<vk::DeviceSize>(mipWidth) *
                static_cast<vk::DeviceSize>(mipHeight) *
                4; // R16G16_SFLOAT = 4 bytes/pixel

            regions.push_back(vk::BufferImageCopy{
                .bufferOffset = offset,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    vk::ImageAspectFlagBits::eColor,
                    mip,
                    0,
                    1
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = {
                    mipWidth,
                    mipHeight,
                    1
                }
            });

            offset += mipSize;
        }

        cmd.copyBufferToImage(
            *stagingBuffer,
            *brdfLutImage,
            vk::ImageLayout::eTransferDstOptimal,
            regions
        );

        transitionImage(
            *brdfLutImage,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferWrite,
            {},
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eBottomOfPipe
        );

        cmd.end();

        vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd
        };

        if (auto r = transferQueue.submit(1, &submitInfo, nullptr);
            r != vk::Result::eSuccess)
        {
            return r;
        }

        transferQueue.waitIdle();

        std::cout << "[PbrRender] BRDF LUT uploaded successfully."
                  << std::endl;

        std::cout
            << "[PbrRender] BRDF LUT "
            << width
            << "x"
            << height
            << " mips="
            << mipCount
            << " bytes="
            << dataSize
            << std::endl;

        return vk::Result::eSuccess;
    }

    void StagingBufferMeshData::recordCopyCommandsToBuffer(
        vk::CommandBuffer cmdBuffer,
        vk::Buffer vertexBuffer,
        vk::Buffer indexBuffer,
        vk::Buffer indirectCommandsBuffer,
        vk::Buffer modelDatasBuffer) const
    {
        cmdBuffer.copyBuffer(stagingBuffer, vertexBuffer, {
            vk::BufferCopy{ .srcOffset = positionAttributeVertexBindingBufferOffset, .dstOffset = 0, .size = vertexBufferSize }
        });
        cmdBuffer.copyBuffer(stagingBuffer, indexBuffer, {
            vk::BufferCopy{ .srcOffset = indicesBufferOffset, .dstOffset = 0, .size = indexBufferSize }
        });
        cmdBuffer.copyBuffer(stagingBuffer, indirectCommandsBuffer, {
            vk::BufferCopy{ .srcOffset = indirectCommandsBufferOffset, .dstOffset = 0, .size = indirectCommandsBufferSize }
        });
        cmdBuffer.copyBuffer(stagingBuffer, modelDatasBuffer, {
            vk::BufferCopy{ .srcOffset = modelDatasBufferOffset, .dstOffset = 0, .size = modelDatasBufferSize }
        });
    }

    vk::ResultValue<DeviceMeshData> PbrRender::prepareDeviceMeshData(
        const StagingBufferMeshData& stagingInfo,
        resources::DeviceAllocator const& deviceAllocator)
    {
        DeviceMeshData deviceMesh;

        auto vbRes = deviceAllocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = stagingInfo.vertexBufferSize,
                .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive
            }, resources::MemoryUsage::eGpuOnly);
        if (vbRes.result != vk::Result::eSuccess) return {vbRes.result, {}};
        deviceMesh.vertexBuffer = std::move(vbRes.value);

        deviceMesh.positionAttributeOffset = 0;
        deviceMesh.normalUvTangentAttributeOffset =
            stagingInfo.normalUvTangentAttributeVertexBindingBufferOffset
            - stagingInfo.positionAttributeVertexBindingBufferOffset;

        auto ibRes = deviceAllocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = stagingInfo.indexBufferSize,
                .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive
            }, resources::MemoryUsage::eGpuOnly);
        if (ibRes.result != vk::Result::eSuccess) return {ibRes.result, {}};
        deviceMesh.indexBuffer = std::move(ibRes.value);
        deviceMesh.indexBufferOffset = 0;

        auto indRes = deviceAllocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = stagingInfo.indirectCommandsBufferSize,
                .usage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive
            }, resources::MemoryUsage::eGpuOnly);
        if (indRes.result != vk::Result::eSuccess) return {indRes.result, {}};
        deviceMesh.indirectBuffer = std::move(indRes.value);
        deviceMesh.indirectBufferOffset = 0;

        auto ssboRes = deviceAllocator.createAndAllocateBufferUnique(
            vk::BufferCreateInfo{
                .size = stagingInfo.modelDatasBufferSize,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive
            }, resources::MemoryUsage::eGpuOnly);
        if (ssboRes.result != vk::Result::eSuccess) return {ssboRes.result, {}};
        deviceMesh.modelSsboBuffer = std::move(ssboRes.value);
        deviceMesh.modelSsboBufferOffset = 0;

        deviceMesh.indirectDraws = stagingInfo.indirectDrawCalls;

        return {vk::Result::eSuccess, std::move(deviceMesh)};
    }

} // namespace shuttle_engine
