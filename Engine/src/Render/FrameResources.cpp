//
// Created by Shagu on 30.07.2026.
//

#include "Render.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Assets/Formats/Geometry.hpp"

namespace shuttle::engine::render
{
    namespace
    {



        uint32_t ceilDiv2(uint32_t v)
        {
            return (v + 1u) / 2u;
        }

        uint32_t calculateHiZCounterCount(
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
        }
    // ============================================================
    // Helpers
    // ============================================================

        uint32_t calculateMipCount(uint32_t width, uint32_t height)
        {
            uint32_t mipCount = 1;

            while (width > 1 || height > 1)
            {
                width = std::max(1u, width / 2u);
                height = std::max(1u, height / 2u);

                ++mipCount;
            }

            return mipCount;
        }

        vk::Result createAllocatedBuffer(
            resources::DeviceAllocator const& allocator,
            vk::DeviceSize size,
            vk::BufferUsageFlags usage,
            resources::MemoryUsage memoryUsage,
            resources::UniqueAllocatedBuffer& outBuffer)
        {
            auto [result, buffer] = allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = size,
                    .usage = usage,
                    .sharingMode = vk::SharingMode::eExclusive},
                memoryUsage);

            if (result != vk::Result::eSuccess) {
                return result;
            }

            outBuffer = std::move(buffer);

            return vk::Result::eSuccess;
        }

        vk::Result createAllocatedImage2D(
            resources::DeviceAllocator const& allocator,
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            uint32_t arrayLayers,
            vk::Format format,
            vk::ImageUsageFlags usage,
            vk::ImageCreateFlags flags,
            resources::UniqueAllocatedImage& outImage)
        {
            auto [result, image] = allocator.createAndAllocateImageUnique(
                vk::ImageCreateInfo{
                    .flags = flags,
                    .imageType = vk::ImageType::e2D,
                    .format = format,
                    .extent = vk::Extent3D{width, height, 1},
                    .mipLevels = mipLevels,
                    .arrayLayers = arrayLayers,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = usage,
                    .sharingMode = vk::SharingMode::eExclusive,
                    .initialLayout = vk::ImageLayout::eUndefined},
                resources::MemoryUsage::eGpuOnly);

            if (result != vk::Result::eSuccess) {
                return result;
            }

            outImage = std::move(image);

            return vk::Result::eSuccess;
        }

        vk::Result createImageView(
            vk::Device device,
            vk::Image image,
            vk::ImageViewType viewType,
            vk::Format format,
            vk::ImageAspectFlags aspectMask,
            uint32_t baseMipLevel,
            uint32_t levelCount,
            uint32_t baseArrayLayer,
            uint32_t layerCount,
            vk::UniqueImageView& outView)
        {
            auto [result, view] = device.createImageViewUnique(
                vk::ImageViewCreateInfo{
                    .image = image,
                    .viewType = viewType,
                    .format = format,
                    .subresourceRange = vk::ImageSubresourceRange{
                        .aspectMask = aspectMask,
                        .baseMipLevel = baseMipLevel,
                        .levelCount = levelCount,
                        .baseArrayLayer = baseArrayLayer,
                        .layerCount = layerCount}});

            if (result != vk::Result::eSuccess) {
                return result;
            }

            outView = std::move(view);

            return vk::Result::eSuccess;
        }

        vk::DescriptorBufferInfo makeBufferInfo(vk::Buffer buffer)
        {
            return vk::DescriptorBufferInfo{
                .buffer = buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE};
        }

        vk::DescriptorImageInfo makeImageInfo(vk::ImageView imageView, vk::ImageLayout layout)
        {
            return vk::DescriptorImageInfo{
                .sampler = nullptr,
                .imageView = imageView,
                .imageLayout = layout};
        }

        vk::Result createDrawListResources(
    resources::DeviceAllocator const& allocator,
    uint32_t commandCount,
    uint32_t maxDrawableCount,
    vk::BufferUsageFlags indirectStorageBufferUsage,
    vk::BufferUsageFlags storageBufferUsage,
    DeviceDrawListResources& outDrawList)
        {
            if (auto result = createAllocatedBuffer(
                    allocator,
                    sizeof(VkDrawIndexedIndirectCommand) * commandCount,
                    indirectStorageBufferUsage,
                    resources::MemoryUsage::eGpuOnly,
                    outDrawList.indirectCommandsBuffer);
                result != vk::Result::eSuccess)
            {
                return result;
            }

            if (auto result = createAllocatedBuffer(
                    allocator,
                    sizeof(uint32_t),
                    indirectStorageBufferUsage,
                    resources::MemoryUsage::eGpuOnly,
                    outDrawList.drawCountBuffer);
                result != vk::Result::eSuccess)
            {
                return result;
            }

            if (auto result = createAllocatedBuffer(
                    allocator,
                    sizeof(MeshRange) * commandCount,
                    storageBufferUsage,
                    resources::MemoryUsage::eGpuOnly,
                    outDrawList.meshRangesBuffer);
                result != vk::Result::eSuccess)
            {
                return result;
            }

            if (auto result = createAllocatedBuffer(
                    allocator,
                    sizeof(uint32_t) * commandCount,
                    storageBufferUsage,
                    resources::MemoryUsage::eGpuOnly,
                    outDrawList.meshWriteCountersBuffer);
                result != vk::Result::eSuccess)
            {
                return result;
            }

            if (auto result = createAllocatedBuffer(
                    allocator,
                    sizeof(uint32_t) * maxDrawableCount,
                    storageBufferUsage,
                    resources::MemoryUsage::eGpuOnly,
                    outDrawList.instanceRemapBuffer);
                result != vk::Result::eSuccess)
            {
                return result;
            }

            return vk::Result::eSuccess;
        }
    } // namespace

    // ============================================================
    // createFrameResources
    // ============================================================

    vk::ResultValue<DeviceFrameResources> createFrameResources(
        RenderContext& context,
        const DeviceRendererResources& rendererResources,
        uint32_t renderWidth,
        uint32_t renderHeight,
        uint32_t drawableCount,
        uint32_t transformCount,
        uint32_t meshCount,
        uint32_t shadowMapResolution,
        uint32_t cascadeCount)
    {
        DeviceFrameResources resources{};

        // ============================================================
        // Constants
        // ============================================================

        constexpr vk::Format LinearDepthFormat = vk::Format::eR32Sfloat;
        constexpr vk::Format HiZFormat = vk::Format::eR32Sfloat;
        constexpr vk::Format GtaoFormat = vk::Format::eR8Unorm;
        const vk::Format depthFormat = context.swapchainDepthFormat;

        const uint32_t commandCount = meshCount * assets::formats::geometry::MaxMeshLods;
        const uint32_t maxDrawableCount = drawableCount;
        const uint32_t maxCandidateCount = drawableCount;
        const uint32_t maxVisibleCandidateCount = drawableCount;
        const uint32_t hiZMipCount =
            std::min(calculateMipCount(renderWidth, renderHeight), static_cast<uint32_t>(MaxHiZMipCount));

        // ============================================================
        // Resource Layout
        // ------------------------------------------------------------
        // CPU-visible:
        //   - frameInfoBuffer
        // GPU-local:
        //   - all other buffers
        //   - all images and image views
        // ============================================================

        // ============================================================
        // Buffer Usage Flags
        // ============================================================

        const vk::BufferUsageFlags storageBufferUsage =
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferSrc |
            vk::BufferUsageFlagBits::eTransferDst;

        const vk::BufferUsageFlags uniformBufferUsage =
            vk::BufferUsageFlagBits::eUniformBuffer |
            vk::BufferUsageFlagBits::eTransferDst;

        const vk::BufferUsageFlags indirectStorageBufferUsage =
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eTransferSrc |
            vk::BufferUsageFlagBits::eTransferDst;

        // ============================================================
        // Frame Data Buffers
        // ============================================================

        // CPU-visible frame constants (persistently mapped).
        {
            auto [result, buffer] = context.allocator.createAndAllocateBufferUnique(
                vk::BufferCreateInfo{
                    .size = sizeof(FrameInfo),
                    .usage = uniformBufferUsage,
                    .sharingMode = vk::SharingMode::eExclusive},
                resources::MemoryUsage::eCpuOnly,
                resources::AllocationCreateFlagBits::eMapped);
            if (result != vk::Result::eSuccess) {
                return {result, {}};
            }
            resources.frameInfoBuffer = std::move(buffer);
        }

        // GPU-local simulation data.
        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(glm::vec4) * 6u * MaxFrustums,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.frustumPlanesBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(DirectionalShadowData),
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.directionalShadowDataBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // GPU-Local Buffers
        // ============================================================

        // ============================================================
        // Scene Update
        // ============================================================

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(glm::mat4) * transformCount,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.worldTransformsBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Frustum Culling
        // ============================================================

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t) * maxCandidateCount,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.candidateIndicesBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t),
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.candidateCountBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Occlusion Pass #1
        // ============================================================

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t) * maxVisibleCandidateCount,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.visibleCandidateIndicesBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t),
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.visibleCandidateCountBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Occlusion Pass #2
        // ============================================================

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t) * maxDrawableCount,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.visibilityFlagsBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t) * maxDrawableCount,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.chosenMeshIdsBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(uint32_t) * maxDrawableCount,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.visibilityMasksBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Draw Command Generation
        // ============================================================

        // ============================================================
        // Independent Draw Lists
        // ============================================================

        if (auto result = createDrawListResources(
                context.allocator,
                commandCount,
                maxDrawableCount,
                indirectStorageBufferUsage,
                storageBufferUsage,
                resources.occluderDrawList);
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        if (auto result = createDrawListResources(
                context.allocator,
                commandCount,
                maxDrawableCount,
                indirectStorageBufferUsage,
                storageBufferUsage,
                resources.visibleDepthDrawList);
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        if (auto result = createDrawListResources(
                context.allocator,
                commandCount,
                maxDrawableCount,
                indirectStorageBufferUsage,
                storageBufferUsage,
                resources.shadowDrawList);
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        if (auto result = createDrawListResources(
                context.allocator,
                commandCount,
                maxDrawableCount,
                indirectStorageBufferUsage,
                storageBufferUsage,
                resources.mainDrawList);
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        // ============================================================
        // Statistics
        // ============================================================

        if (auto result = createAllocatedBuffer(
                context.allocator,
                sizeof(RenderStatistics),
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.renderStatisticsBuffer);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        const uint32_t hiZCounterCount =
            calculateHiZCounterCount(
                renderWidth,
                renderHeight,
                MaxHiZMipCount);

        const vk::DeviceSize hiZCounterBufferSize =
            sizeof(uint32_t) * std::max(1u, hiZCounterCount);

        if (auto result = createAllocatedBuffer(
                context.allocator,
                hiZCounterBufferSize,
                storageBufferUsage,
                resources::MemoryUsage::eGpuOnly,
                resources.hizCountersBuffer);
            result != vk::Result::eSuccess)
        {
            return {result, {}};
        }

        std::cout
    << "[HiZ] mipCount=" << hiZMipCount
    << " counterCount=" << hiZCounterCount
    << " counterBytes=" << hiZCounterBufferSize
    << "\n";

        // ============================================================
        // GPU-Local Images
        // ============================================================

        // ============================================================
        // Depth Image
        // ============================================================

        if (auto result = createAllocatedImage2D(
                context.allocator,
                renderWidth,
                renderHeight,
                1,
                1,
                depthFormat,
                vk::ImageUsageFlagBits::eDepthStencilAttachment |
                    vk::ImageUsageFlagBits::eSampled,
                {},
                resources.depthImage);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.depthImage,
                vk::ImageViewType::e2D,
                depthFormat,
                vk::ImageAspectFlagBits::eDepth,
                0,
                1,
                0,
                1,
                resources.depthImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Linear Depth Image
        // ============================================================

        if (auto result = createAllocatedImage2D(
                context.allocator,
                renderWidth,
                renderHeight,
                1,
                1,
                LinearDepthFormat,
                vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eStorage,
                {},
                resources.linearDepthImage);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.linearDepthImage,
                vk::ImageViewType::e2D,
                LinearDepthFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
                resources.linearDepthImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.linearDepthImage,
                vk::ImageViewType::e2D,
                LinearDepthFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
                resources.linearDepthStorageImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Hi-Z Image
        // ============================================================

        if (auto result = createAllocatedImage2D(
                context.allocator,
                renderWidth,
                renderHeight,
                hiZMipCount,
                1,
                HiZFormat,
                vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eStorage,
                {},
                resources.hizImage);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.hizImage,
                vk::ImageViewType::e2D,
                HiZFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                hiZMipCount,
                0,
                1,
                resources.hizFullView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        resources.hizMipViews.reserve(hiZMipCount);
        resources.hizStorageMipViews.reserve(hiZMipCount);

        for (uint32_t mip = 0; mip < hiZMipCount; ++mip) {
            vk::UniqueImageView sampledView;
            vk::UniqueImageView storageView;

            if (auto result = createImageView(
                    context.device,
                    *resources.hizImage,
                    vk::ImageViewType::e2D,
                    HiZFormat,
                    vk::ImageAspectFlagBits::eColor,
                    mip,
                    1,
                    0,
                    1,
                    sampledView);
                result != vk::Result::eSuccess) {
                return {result, {}};
            }

            if (auto result = createImageView(
                    context.device,
                    *resources.hizImage,
                    vk::ImageViewType::e2D,
                    HiZFormat,
                    vk::ImageAspectFlagBits::eColor,
                    mip,
                    1,
                    0,
                    1,
                    storageView);
                result != vk::Result::eSuccess) {
                return {result, {}};
            }
            resources.hizMipViews.push_back(std::move(sampledView));
            resources.hizStorageMipViews.push_back(std::move(storageView));
        }

        // ============================================================
        // GTAO Images
        // ============================================================

        if (auto result = createAllocatedImage2D(
                context.allocator,
                renderWidth,
                renderHeight,
                1,
                1,
                GtaoFormat,
                vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eStorage,
                {},
                resources.gtaoImage);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.gtaoImage,
                vk::ImageViewType::e2D,
                GtaoFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
                resources.gtaoImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.gtaoImage,
                vk::ImageViewType::e2D,
                GtaoFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
                resources.gtaoStorageImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createAllocatedImage2D(
                context.allocator,
                renderWidth,
                renderHeight,
                1,
                1,
                GtaoFormat,
                vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eStorage,
                {},
                resources.gtaoFilteredImage);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.gtaoFilteredImage,
                vk::ImageViewType::e2D,
                GtaoFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
                resources.gtaoFilteredImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.gtaoFilteredImage,
                vk::ImageViewType::e2D,
                GtaoFormat,
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
                resources.gtaoFilteredStorageImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Shadow Map Array
        // ============================================================

        if (auto result = createAllocatedImage2D(
                context.allocator,
                shadowMapResolution,
                shadowMapResolution,
                1,
                cascadeCount,
                depthFormat,
                vk::ImageUsageFlagBits::eDepthStencilAttachment |
                    vk::ImageUsageFlagBits::eSampled,
                {},
                resources.shadowMapImage);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        if (auto result = createImageView(
                context.device,
                *resources.shadowMapImage,
                vk::ImageViewType::e2DArray,
                depthFormat,
                vk::ImageAspectFlagBits::eDepth,
                0,
                1,
                0,
                cascadeCount,
                resources.shadowMapImageView);
            result != vk::Result::eSuccess) {
            return {result, {}};
        }

        // ============================================================
        // Allocate Frame Descriptor Set
        // ============================================================

        vk::DescriptorSetLayout frameSetLayout = *rendererResources.frameSetLayout;

        // ============================================================
// Allocate Frame Descriptor Sets
// ============================================================
//
// Все 4 descriptor set используют один и тот же frameSetLayout.
// Отличаются только draw-list буферы внутри bindings:
//   FRAME_INDIRECT_COMMANDS
//   FRAME_DRAW_COUNT
//   FRAME_MESH_RANGES
//   FRAME_MESH_WRITE_COUNTERS
//   FRAME_INSTANCE_REMAP

std::array frameSetLayouts{
    frameSetLayout,
    frameSetLayout,
    frameSetLayout,
    frameSetLayout
};

vk::DescriptorSetAllocateInfo allocateInfo{
    .descriptorPool = *context.descriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(frameSetLayouts.size()),
    .pSetLayouts = frameSetLayouts.data()
};

auto [allocateResult, frameSets] =
    context.device.allocateDescriptorSets(allocateInfo);

if (allocateResult != vk::Result::eSuccess)
{
    return {allocateResult, {}};
}

resources.occluderFrameSet = frameSets[0];
resources.visibleDepthFrameSet = frameSets[1];
resources.shadowFrameSet = frameSets[2];
resources.mainFrameSet = frameSets[3];

// ============================================================
// Descriptor Buffer Infos: shared frame resources
// ============================================================

const auto frameInfo =
    makeBufferInfo(*resources.frameInfoBuffer);

const auto frustumPlanesInfo =
    makeBufferInfo(*resources.frustumPlanesBuffer);

const auto directionalShadowDataInfo =
    makeBufferInfo(*resources.directionalShadowDataBuffer);

const auto worldTransformsInfo =
    makeBufferInfo(*resources.worldTransformsBuffer);

const auto candidateIndicesInfo =
    makeBufferInfo(*resources.candidateIndicesBuffer);

const auto candidateCountInfo =
    makeBufferInfo(*resources.candidateCountBuffer);

const auto visibleCandidateIndicesInfo =
    makeBufferInfo(*resources.visibleCandidateIndicesBuffer);

const auto visibleCandidateCountInfo =
    makeBufferInfo(*resources.visibleCandidateCountBuffer);

const auto visibilityFlagsInfo =
    makeBufferInfo(*resources.visibilityFlagsBuffer);

const auto chosenMeshIdsInfo =
    makeBufferInfo(*resources.chosenMeshIdsBuffer);

const auto renderStatisticsInfo =
    makeBufferInfo(*resources.renderStatisticsBuffer);

const auto hizCountersInfo =
    makeBufferInfo(*resources.hizCountersBuffer);

const auto visibilityMasksInfo =
    makeBufferInfo(*resources.visibilityMasksBuffer);

// ============================================================
// Descriptor Image Infos
// ============================================================

const auto depthImageInfo =
    makeImageInfo(
        *resources.depthImageView,
        vk::ImageLayout::eDepthStencilReadOnlyOptimal);

const auto linearDepthImageInfo =
    makeImageInfo(
        *resources.linearDepthImageView,
        vk::ImageLayout::eShaderReadOnlyOptimal);

const auto linearDepthStorageInfo =
    makeImageInfo(
        *resources.linearDepthStorageImageView,
        vk::ImageLayout::eGeneral);

const auto hizFullImageInfo =
    makeImageInfo(
        *resources.hizFullView,
        vk::ImageLayout::eShaderReadOnlyOptimal);

std::vector<vk::DescriptorImageInfo> hizStorageInfos(MaxHiZMipCount);

for (uint32_t i = 0; i < MaxHiZMipCount; ++i)
{
    const uint32_t mip =
        std::min(i, hiZMipCount - 1u);

    hizStorageInfos[i] =
        makeImageInfo(
            *resources.hizStorageMipViews[mip],
            vk::ImageLayout::eGeneral);
}

const auto gtaoImageInfo =
    makeImageInfo(
        *resources.gtaoImageView,
        vk::ImageLayout::eShaderReadOnlyOptimal);

const auto gtaoStorageInfo =
    makeImageInfo(
        *resources.gtaoStorageImageView,
        vk::ImageLayout::eGeneral);

const auto gtaoFilteredImageInfo =
    makeImageInfo(
        *resources.gtaoFilteredImageView,
        vk::ImageLayout::eShaderReadOnlyOptimal);

const auto gtaoFilteredStorageInfo =
    makeImageInfo(
        *resources.gtaoFilteredStorageImageView,
        vk::ImageLayout::eGeneral);

const auto shadowMapImageInfo =
    makeImageInfo(
        *resources.shadowMapImageView,
        vk::ImageLayout::eDepthStencilReadOnlyOptimal);

// ============================================================
// Update one frame descriptor set with selected draw-list
// ============================================================

using FB = FrameBindings;

auto updateFrameDescriptorSet = [&](vk::DescriptorSet dstSet, const DeviceDrawListResources& drawList)
{
    const auto indirectCommandsInfo =
        makeBufferInfo(*drawList.indirectCommandsBuffer);

    const auto drawCountInfo =
        makeBufferInfo(*drawList.drawCountBuffer);

    const auto meshRangesInfo =
        makeBufferInfo(*drawList.meshRangesBuffer);

    const auto meshWriteCountersInfo =
        makeBufferInfo(*drawList.meshWriteCountersBuffer);

    const auto instanceRemapInfo =
        makeBufferInfo(*drawList.instanceRemapBuffer);

    std::vector<vk::WriteDescriptorSet> writes;

    auto writeBuffer = [&writes, dstSet](FB binding, vk::DescriptorType type, const vk::DescriptorBufferInfo& info)
    {
        writes.push_back(vk::WriteDescriptorSet{
            .dstSet = dstSet,
            .dstBinding = static_cast<uint32_t>(binding),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pBufferInfo = &info
        });
    };

    auto writeImage = [&]
        (FB binding, vk::DescriptorType type, const vk::DescriptorImageInfo& info)
    {
        writes.push_back(vk::WriteDescriptorSet{
            .dstSet = dstSet,
            .dstBinding = static_cast<uint32_t>(binding),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = &info
        });
    };

    // ============================================================
    // Shared buffers
    // ============================================================

    writeBuffer(
        FB::eFrameInfo,
        vk::DescriptorType::eUniformBuffer,
        frameInfo);

    writeBuffer(
        FB::eFrustumPlanes,
        vk::DescriptorType::eStorageBuffer,
        frustumPlanesInfo);

    writeBuffer(
        FB::eDirectionalShadowData,
        vk::DescriptorType::eStorageBuffer,
        directionalShadowDataInfo);

    writeBuffer(
        FB::eWorldTransforms,
        vk::DescriptorType::eStorageBuffer,
        worldTransformsInfo);

    writeBuffer(
        FB::eCandidateIndices,
        vk::DescriptorType::eStorageBuffer,
        candidateIndicesInfo);

    writeBuffer(
        FB::eCandidateCount,
        vk::DescriptorType::eStorageBuffer,
        candidateCountInfo);

    writeBuffer(
        FB::eVisibleCandidateIndices,
        vk::DescriptorType::eStorageBuffer,
        visibleCandidateIndicesInfo);

    writeBuffer(
        FB::eVisibleCandidateCount,
        vk::DescriptorType::eStorageBuffer,
        visibleCandidateCountInfo);

    writeBuffer(
        FB::eVisibilityFlags,
        vk::DescriptorType::eStorageBuffer,
        visibilityFlagsInfo);

    writeBuffer(
        FB::eChosenMeshIds,
        vk::DescriptorType::eStorageBuffer,
        chosenMeshIdsInfo);

    // ============================================================
    // Per-draw-list buffers
    // ============================================================

    writeBuffer(
        FB::eIndirectCommands,
        vk::DescriptorType::eStorageBuffer,
        indirectCommandsInfo);

    writeBuffer(
        FB::eDrawCount,
        vk::DescriptorType::eStorageBuffer,
        drawCountInfo);

    writeBuffer(
        FB::eMeshRanges,
        vk::DescriptorType::eStorageBuffer,
        meshRangesInfo);

    writeBuffer(
        FB::eMeshWriteCounters,
        vk::DescriptorType::eStorageBuffer,
        meshWriteCountersInfo);

    writeBuffer(
        FB::eInstanceRemap,
        vk::DescriptorType::eStorageBuffer,
        instanceRemapInfo);

    // ============================================================
    // Shared stats / masks / Hi-Z
    // ============================================================

    writeBuffer(
        FB::eRenderStatistics,
        vk::DescriptorType::eStorageBuffer,
        renderStatisticsInfo);

    writeBuffer(
        FB::eHiZCounters,
        vk::DescriptorType::eStorageBuffer,
        hizCountersInfo);

    writeBuffer(
        FB::eVisibilityMasks,
        vk::DescriptorType::eStorageBuffer,
        visibilityMasksInfo);

    // ============================================================
    // Sampled / storage images
    // ============================================================

    writeImage(
        FB::eDepthImage,
        vk::DescriptorType::eSampledImage,
        depthImageInfo);

    writeImage(
        FB::eLinearDepthImage,
        vk::DescriptorType::eSampledImage,
        linearDepthImageInfo);

    writeImage(
        FB::eLinearDepthStorageImage,
        vk::DescriptorType::eStorageImage,
        linearDepthStorageInfo);

    writeImage(
        FB::eHiZPyramidImage,
        vk::DescriptorType::eSampledImage,
        hizFullImageInfo);

    writes.push_back(vk::WriteDescriptorSet{
        .dstSet = dstSet,
        .dstBinding = static_cast<uint32_t>(FB::eHiZStorageImages),
        .dstArrayElement = 0,
        .descriptorCount = MaxHiZMipCount,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .pImageInfo = hizStorageInfos.data()
    });

    writeImage(
        FB::eGtaoImage,
        vk::DescriptorType::eSampledImage,
        gtaoImageInfo);

    writeImage(
        FB::eGtaoStorageImage,
        vk::DescriptorType::eStorageImage,
        gtaoStorageInfo);

    writeImage(
        FB::eGtaoFilteredImage,
        vk::DescriptorType::eSampledImage,
        gtaoFilteredImageInfo);

    writeImage(
        FB::eGtaoFilteredStorageImage,
        vk::DescriptorType::eStorageImage,
        gtaoFilteredStorageInfo);

    writeImage(
        FB::eDirectionalShadowMapImage,
        vk::DescriptorType::eSampledImage,
        shadowMapImageInfo);

    context.device.updateDescriptorSets(writes, {});
};

// ============================================================
// Update all four frame descriptor sets
// ============================================================

updateFrameDescriptorSet(
    resources.occluderFrameSet,
    resources.occluderDrawList);

updateFrameDescriptorSet(
    resources.visibleDepthFrameSet,
    resources.visibleDepthDrawList);

updateFrameDescriptorSet(
    resources.shadowFrameSet,
    resources.shadowDrawList);

updateFrameDescriptorSet(
    resources.mainFrameSet,
    resources.mainDrawList);

        return {vk::Result::eSuccess, std::move(resources)};
    }

    vk::ResultValue<std::vector<RenderTargets>> createRenderTargets(
        vk::Device device,
        resources::DeviceAllocator const&,
        std::vector<vk::Image> renderTargetImages,
        vk::Extent2D swapchainExtent,
        vk::Format swapchainFormat)
    {
        std::vector<RenderTargets> renderTargets;
        renderTargets.reserve(renderTargetImages.size());

        for (uint32_t imageIndex = 0; imageIndex < renderTargetImages.size(); ++imageIndex) {
            vk::UniqueImageView imageView;
            if (auto result = createImageView(
                    device,
                    renderTargetImages[imageIndex],
                    vk::ImageViewType::e2D,
                    swapchainFormat,
                    vk::ImageAspectFlagBits::eColor,
                    0,
                    1,
                    0,
                    1,
                    imageView);
                result != vk::Result::eSuccess) {
                return {result, {}};
            }

            renderTargets.push_back(
                RenderTargets{
                    .renderTargetExtent = swapchainExtent,
                    .colorAttachmentImage = renderTargetImages[imageIndex],
                    .colorAttachmentImageView = std::move(imageView),
                    .swapchainImageIndex = imageIndex});
        }

        return {vk::Result::eSuccess, std::move(renderTargets)};
    }
} // namespace shuttle::engine::render
