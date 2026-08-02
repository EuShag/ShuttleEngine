//
// Created by Shagu on 29.07.2026.
//
#include "Render.hpp"
#include <fstream>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"

namespace shuttle::engine::render
{

namespace
{

// ============================================================
// Small local helper
// ============================================================

struct UploadedImage2D
{
    resources::UniqueAllocatedImage image;
    vk::UniqueImageView imageView;
};

// ============================================================
// Upload generic 2D image data
// ============================================================

vk::Result uploadImage2D(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
                         resources::DeviceAllocator const& allocator,

                         vk::Format format, uint32_t width, uint32_t height, uint32_t mipCount,

                         std::span<const std::byte> imageBytes, std::span<const vk::BufferImageCopy> copyRegions,

                         UploadedImage2D& outImage)
{
    // ============================================================
    // Staging Buffer
    // ============================================================

    auto [stagingResult, stagingBuffer] =
        allocator.createAndAllocateBufferUnique(vk::BufferCreateInfo{.size = imageBytes.size_bytes(),
                                                                     .usage = vk::BufferUsageFlagBits::eTransferSrc,
                                                                     .sharingMode = vk::SharingMode::eExclusive},
                                                resources::MemoryUsage::eCpuOnly);

    if (stagingResult != vk::Result::eSuccess)
    {
        return stagingResult;
    }

    if (auto result = allocator.writeBufferFromHost({
            .dstBuffer = *stagingBuffer,
            .dstBufferOffset = 0,
            .srcData = imageBytes.data(),
            .dataSize = imageBytes.size_bytes(),
        });
        result != vk::Result::eSuccess)
    {
        return result;
    }

    // ============================================================
    // GPU Image
    // ============================================================

    auto [imageResult, image] = allocator.createAndAllocateImageUnique(
        vk::ImageCreateInfo{.imageType = vk::ImageType::e2D,
                            .format = format,
                            .extent = vk::Extent3D{width, height, 1},
                            .mipLevels = mipCount,
                            .arrayLayers = 1,
                            .samples = vk::SampleCountFlagBits::e1,
                            .tiling = vk::ImageTiling::eOptimal,
                            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                            .sharingMode = vk::SharingMode::eExclusive,
                            .initialLayout = vk::ImageLayout::eUndefined},
        resources::MemoryUsage::eGpuOnly);

    if (imageResult != vk::Result::eSuccess)
    {
        return imageResult;
    }

    // ============================================================
    // Image View
    // ============================================================

    auto [viewResult, imageView] = device.createImageViewUnique(vk::ImageViewCreateInfo{
        .image = *image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                      .baseMipLevel = 0,
                                                      .levelCount = mipCount,
                                                      .baseArrayLayer = 0,
                                                      .layerCount = 1}});

    if (viewResult != vk::Result::eSuccess)
    {
        return viewResult;
    }

    // ============================================================
    // One-time command buffer
    // ============================================================

    auto [cmdResult, commandBuffers] = device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
        .commandPool = transferCommandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1});

    if (cmdResult != vk::Result::eSuccess)
    {
        return cmdResult;
    }

    vk::CommandBuffer cmd = commandBuffers[0].get();

    if (auto cmdBeginResult =
            cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        cmdBeginResult != vk::Result::eSuccess)
        return cmdBeginResult;

    auto transitionImage = [&cmd, mipCount](vk::Image img, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                            vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
                                            vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage)
    {
        vk::ImageMemoryBarrier barrier{.srcAccessMask = srcAccess,
                                       .dstAccessMask = dstAccess,
                                       .oldLayout = oldLayout,
                                       .newLayout = newLayout,
                                       .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                                       .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                                       .image = img,
                                       .subresourceRange =
                                           vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                                     .baseMipLevel = 0,
                                                                     .levelCount = mipCount,
                                                                     .baseArrayLayer = 0,
                                                                     .layerCount = 1}};

        cmd.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
    };

    transitionImage(*image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                    vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::PipelineStageFlagBits::eTransfer);

    cmd.copyBufferToImage(*stagingBuffer, *image, vk::ImageLayout::eTransferDstOptimal, copyRegions);

    transitionImage(*image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::AccessFlagBits::eTransferWrite, {}, vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eBottomOfPipe);

    if (auto cmdEndResult = cmd.end(); cmdEndResult != vk::Result::eSuccess) return cmdEndResult;

    vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &cmd};

    if (auto result = transferQueue.submit(1, &submitInfo, nullptr); result != vk::Result::eSuccess)
    {
        return result;
    }

    if (auto waitResult = transferQueue.waitIdle(); waitResult != vk::Result::eSuccess) return waitResult;

    outImage.image = std::move(image);

    outImage.imageView = std::move(imageView);

    return vk::Result::eSuccess;
}

// ============================================================
// Create 1x1 fallback texture
// ============================================================

vk::Result createFallbackTexture1x1(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
                                    resources::DeviceAllocator const& allocator,

                                    std::array<std::byte, 4> rgba,

                                    resources::UniqueAllocatedImage& outImage, vk::UniqueImageView& outImageView)
{
    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            vk::ImageSubresourceLayers{
                .aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageOffset = vk::Offset3D{0, 0, 0},
        .imageExtent = vk::Extent3D{1, 1, 1}};

    UploadedImage2D uploaded{};

    const vk::Result result =
        uploadImage2D(device, transferQueue, transferCommandPool, allocator, vk::Format::eR8G8B8A8Unorm, 1, 1, 1,
                      std::span<const std::byte>(rgba.data(), rgba.size()),
                      std::span<const vk::BufferImageCopy>(&region, 1), uploaded);

    if (result != vk::Result::eSuccess)
    {
        return result;
    }

    outImage = std::move(uploaded.image);

    outImageView = std::move(uploaded.imageView);

    return vk::Result::eSuccess;
}

// ============================================================
// Load BRDF LUT DDS
// ============================================================

vk::Result loadBrdfLutDds(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
                          resources::DeviceAllocator const& allocator, DeviceRendererResources& rendererResources)
{
    const std::filesystem::path brdfPath = "../resources/engine/brdf_lut.dds";

    std::cout << "[Renderer] Loading BRDF LUT: " << brdfPath.string() << std::endl;

    tinyddsloader::DDSFile ddsFile;

    const std::string pathString = brdfPath.string();

    auto loadResult = ddsFile.Load(pathString.c_str());

    if (loadResult != tinyddsloader::Result::Success)
    {
        std::cerr << "[Renderer] Failed to load BRDF LUT DDS." << std::endl;

        return vk::Result::eErrorInitializationFailed;
    }

    assert(ddsFile.GetFormat() == tinyddsloader::DDSFile::DXGIFormat::R16G16_Float);

    const uint32_t width = ddsFile.GetWidth();

    const uint32_t height = ddsFile.GetHeight();

    const uint32_t mipCount = ddsFile.GetMipCount();

    constexpr auto format = vk::Format::eR16G16Sfloat;

    // ============================================================
    // Collect DDS bytes
    // ============================================================

    vk::DeviceSize totalSize = 0;

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        const auto* imageData = ddsFile.GetImageData(mip, 0);

        totalSize += imageData->m_memSlicePitch;
    }

    std::vector<std::byte> textureBytes;
    textureBytes.resize(static_cast<size_t>(totalSize));

    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(mipCount);

    std::byte* dst = textureBytes.data();

    vk::DeviceSize offset = 0;

    for (uint32_t mip = 0; mip < mipCount; ++mip)
    {
        const auto* imageData = ddsFile.GetImageData(mip, 0);

        std::memcpy(dst, imageData->m_mem, imageData->m_memSlicePitch);

        dst += imageData->m_memSlicePitch;

        const uint32_t mipWidth = std::max(1u, width >> mip);

        const uint32_t mipHeight = std::max(1u, height >> mip);

        regions.push_back(vk::BufferImageCopy{
            .bufferOffset = offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = vk::ImageSubresourceLayers{.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                           .mipLevel = mip,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1},
            .imageOffset = vk::Offset3D{0, 0, 0},
            .imageExtent = vk::Extent3D{mipWidth, mipHeight, 1}});

        offset += imageData->m_memSlicePitch;
    }

    UploadedImage2D uploaded{};

    const vk::Result uploadResult =
        uploadImage2D(device, transferQueue, transferCommandPool, allocator, format, width, height, mipCount,
                      std::span<const std::byte>(textureBytes.data(), textureBytes.size()),
                      std::span<const vk::BufferImageCopy>(regions.data(), regions.size()), uploaded);

    if (uploadResult != vk::Result::eSuccess)
    {
        return uploadResult;
    }

    rendererResources.brdfLutImage = std::move(uploaded.image);

    rendererResources.brdfLutImageView = std::move(uploaded.imageView);

    std::cout << "[Renderer] BRDF LUT uploaded: " << width << "x" << height << " mips=" << mipCount
              << " bytes=" << totalSize << std::endl;

    return vk::Result::eSuccess;
}

// ============================================================
// Public function
// ============================================================

vk::Result createBuiltinRendererImages(vk::Device device, vk::Queue transferQueue, vk::CommandPool transferCommandPool,
                                       resources::DeviceAllocator const& allocator,
                                       DeviceRendererResources& rendererResources)
{
    // ============================================================
    // BRDF LUT
    // ============================================================

    if (const vk::Result result =
            loadBrdfLutDds(device, transferQueue, transferCommandPool, allocator, rendererResources);
        result != vk::Result::eSuccess)
    {
        return result;
    }

    // ============================================================
    // Fallback Albedo
    // ============================================================
    //
    // White RGBA.
    //
    // Used when material has no albedo texture.

    if (const vk::Result result =
            createFallbackTexture1x1(device, transferQueue, transferCommandPool, allocator,
                                     std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}},
                                     rendererResources.fallbackAlbedoImage, rendererResources.fallbackAlbedoImageView);
        result != vk::Result::eSuccess)
    {
        return result;
    }

    // ============================================================
    // Fallback Normal
    // ============================================================
    //
    // Tangent-space flat normal:
    //
    // RGB = (0.5, 0.5, 1.0)

    if (const vk::Result result =
            createFallbackTexture1x1(device, transferQueue, transferCommandPool, allocator,
                                     std::array{std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}},
                                     rendererResources.fallbackNormalImage, rendererResources.fallbackNormalImageView);
        result != vk::Result::eSuccess)
    {
        return result;
    }

    // ============================================================
    // Fallback ORM
    // ============================================================
    //
    // R = AO        = 1.0
    // G = Roughness = 1.0
    // B = Metallic  = 0.0
    // A = unused    = 1.0

    if (const vk::Result result =
            createFallbackTexture1x1(device, transferQueue, transferCommandPool, allocator,
                                     std::array{std::byte{255}, std::byte{255}, std::byte{0}, std::byte{255}},
                                     rendererResources.fallbackOrmImage, rendererResources.fallbackOrmImageView);
        result != vk::Result::eSuccess)
    {
        return result;
    }

    // ============================================================
    // Fallback Emission
    // ============================================================
    //
    // Black RGBA.

    if (const vk::Result result = createFallbackTexture1x1(
            device, transferQueue, transferCommandPool, allocator,
            std::array{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255}},
            rendererResources.fallbackEmissionImage, rendererResources.fallbackEmissionImageView);
        result != vk::Result::eSuccess)
    {
        return result;
    }

    std::cout << "[Renderer] Builtin renderer images created." << std::endl;

    return vk::Result::eSuccess;
}

std::vector<std::byte> loadBinaryFile(const std::filesystem::path& path)
{

    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) throw std::runtime_error("Failed to open file: " + path.string());

    const auto size = static_cast<size_t>(file.tellg());

    std::vector<std::byte> buffer(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

    return buffer;
}

vk::ResultValue<vk::UniqueDescriptorSetLayout> createRendererDescriptorSetLayout(vk::Device device)
{

    using RB = RendererBindings;

    std::vector<vk::DescriptorSetLayoutBinding> bindings{
        // ============================================================
        // Material Sampler
        // ============================================================

        {.binding = static_cast<uint32_t>(RB::eMaterialSampler),
         .descriptorType = vk::DescriptorType::eSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment},

        // ============================================================
        // Shadow Sampler
        // ============================================================

        {.binding = static_cast<uint32_t>(RB::eShadowSampler),
         .descriptorType = vk::DescriptorType::eSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment},

        // ============================================================
        // Nearest Sampler
        // ============================================================

        {.binding = static_cast<uint32_t>(RB::eNearestSampler),
         .descriptorType = vk::DescriptorType::eSampler,
         .descriptorCount = 1,
         .stageFlags =
             vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // BRDF LUT Image
        // ============================================================

        {.binding = static_cast<uint32_t>(RB::eBrdfLutImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment}};

    vk::DescriptorSetLayoutCreateInfo createInfo{.bindingCount = static_cast<uint32_t>(bindings.size()),
                                                 .pBindings = bindings.data()};

    return device.createDescriptorSetLayoutUnique(createInfo);
}

vk::ResultValue<vk::UniqueDescriptorSetLayout> createSceneDescriptorSetLayout(vk::Device device)
{
    using SB = SceneBindings;

    std::vector<vk::DescriptorSetLayoutBinding> bindings{
        // ============================================================
        // Scene Nodes
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eNodes),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Node Levels
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eNodeLevels),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Local Scene Transforms
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eTransforms),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Drawable Objects
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eDrawables),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Positions
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::ePositions),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Attributes
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eAttributes),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Meshes
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eMeshes),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Indices
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eIndices),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Materials
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eMaterials),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Directional Lights
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eDirectionalLights),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Scene Info
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eSceneInfo),
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags =
             vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Bindless Textures
        // ============================================================

        {.binding = static_cast<uint32_t>(SB::eTextures),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 2048,
         .stageFlags = vk::ShaderStageFlagBits::eFragment}};

    // ============================================================
    // Descriptor indexing flags for bindless textures
    // ============================================================

    std::vector bindingFlags(bindings.size(), vk::DescriptorBindingFlags{});

    // Последний binding в массиве bindings это eTextures.
    bindingFlags.back() =
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    // Если хочешь variable descriptor count, можно добавить:
    //
    // vk::DescriptorBindingFlagBits::eVariableDescriptorCount
    //
    // Но для фиксированных 2048 текстур он не обязателен.

    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .bindingCount = static_cast<uint32_t>(bindingFlags.size()), .pBindingFlags = bindingFlags.data()};

    vk::DescriptorSetLayoutCreateInfo createInfo{.pNext = &bindingFlagsInfo,
                                                 .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
                                                 .bindingCount = static_cast<uint32_t>(bindings.size()),
                                                 .pBindings = bindings.data()};

    auto result = device.createDescriptorSetLayoutUnique(createInfo);
    return std::move(result);
}

vk::ResultValue<vk::UniqueDescriptorSetLayout> createEnvironmentDescriptorSetLayout(vk::Device device)
{
    using EB = EnvironmentBindings;

    std::vector<vk::DescriptorSetLayoutBinding> bindings{// ============================================================
                                                         // Skybox
                                                         // ============================================================

                                                         {.binding = static_cast<uint32_t>(EB::eSkyboxImage),
                                                          .descriptorType = vk::DescriptorType::eSampledImage,
                                                          .descriptorCount = 1,
                                                          .stageFlags = vk::ShaderStageFlagBits::eFragment},

                                                         // ============================================================
                                                         // Irradiance
                                                         // ============================================================

                                                         {.binding = static_cast<uint32_t>(EB::eIrradianceImage),
                                                          .descriptorType = vk::DescriptorType::eSampledImage,
                                                          .descriptorCount = 1,
                                                          .stageFlags = vk::ShaderStageFlagBits::eFragment},

                                                         // ============================================================
                                                         // Radiance
                                                         // ============================================================

                                                         {.binding = static_cast<uint32_t>(EB::eRadianceImage),
                                                          .descriptorType = vk::DescriptorType::eSampledImage,
                                                          .descriptorCount = 1,
                                                          .stageFlags = vk::ShaderStageFlagBits::eFragment}};

    vk::DescriptorSetLayoutCreateInfo createInfo{.bindingCount = static_cast<uint32_t>(bindings.size()),
                                                 .pBindings = bindings.data()};

    return device.createDescriptorSetLayoutUnique(createInfo);
}

vk::ResultValue<vk::UniqueDescriptorSetLayout> createFrameDescriptorSetLayout(vk::Device device)
{
    using FB = FrameBindings;

    constexpr vk::ShaderStageFlags allGraphicsAndCompute =
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;

    constexpr vk::ShaderStageFlags computeOnly = vk::ShaderStageFlagBits::eCompute;

    constexpr vk::ShaderStageFlags fragmentOnly = vk::ShaderStageFlagBits::eFragment;

    constexpr vk::ShaderStageFlags fragmentAndCompute =
        vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;

    std::vector<vk::DescriptorSetLayoutBinding> bindings{
        // ============================================================
        // FrameInfo UBO
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eFrameInfo),
         .descriptorType = vk::DescriptorType::eUniformBuffer,
         .descriptorCount = 1,
         .stageFlags = allGraphicsAndCompute},

        // ============================================================
        // Frustum Planes
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eFrustumPlanes),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Directional Shadow Data
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eDirectionalShadowData),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = allGraphicsAndCompute},

        // ============================================================
        // World Transforms
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eWorldTransforms),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = allGraphicsAndCompute},

        // ============================================================
        // Candidate Indices
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eCandidateIndices),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Candidate Count
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eCandidateCount),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Visibility Flags
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eVisibilityFlags),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Chosen Mesh IDs
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eChosenMeshIds),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Indirect Commands
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eIndirectCommands),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Draw Count
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eDrawCount),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Mesh Ranges
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eMeshRanges),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Mesh Write Counters
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eMeshWriteCounters),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Instance Remap
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eInstanceRemap),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Depth Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eDepthImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = fragmentAndCompute},

        // ============================================================
        // Linear Depth Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eLinearDepthImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = fragmentAndCompute},

        // ============================================================
        // Hi-Z Pyramid Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eHiZPyramidImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = fragmentAndCompute},

        // ============================================================
        // GTAO Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eGtaoImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = fragmentAndCompute},

        // ============================================================
        // GTAO Filtered Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eGtaoFilteredImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = fragmentAndCompute},

        // ============================================================
        // Directional Shadow Map Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eDirectionalShadowMapImage),
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = fragmentOnly},

        // ============================================================
        // Render Statistics
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eRenderStatistics),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Linear Depth Storage Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eLinearDepthStorageImage),
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Hi-Z Storage Images
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eHiZStorageImages),
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = MaxHiZMipCount,
         .stageFlags = computeOnly},

        // ============================================================
        // GTAO Storage Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eGtaoStorageImage),
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // GTAO Filtered Storage Image
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eGtaoFilteredStorageImage),
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Hi-Z Counters
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eHiZCounters),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Visibility Masks
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eVisibilityMasks),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},

        // ============================================================
        // Visible Candidate Indices
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eVisibleCandidateIndices),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly},

        // ============================================================
        // Visible Candidate Count
        // ============================================================

        {.binding = static_cast<uint32_t>(FB::eVisibleCandidateCount),
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = computeOnly}};

    vk::DescriptorSetLayoutCreateInfo createInfo{.bindingCount = static_cast<uint32_t>(bindings.size()),
                                                 .pBindings = bindings.data()};

    return device.createDescriptorSetLayoutUnique(createInfo);
}

vk::ResultValue<vk::UniquePipelineLayout> createPipelineLayout(vk::Device device,
                                                               vk::DescriptorSetLayout rendererSetLayout,
                                                               vk::DescriptorSetLayout environmentSetLayout,
                                                               vk::DescriptorSetLayout sceneSetLayout,
                                                               vk::DescriptorSetLayout frameSetLayout)
{

    std::array descriptorSetLayouts{rendererSetLayout, environmentSetLayout, sceneSetLayout, frameSetLayout};

    vk::PushConstantRange pushConstantRange{.stageFlags = vk::ShaderStageFlagBits::eVertex |
                                                          vk::ShaderStageFlagBits::eFragment |
                                                          vk::ShaderStageFlagBits::eCompute,

                                            .offset = 0,
                                            .size = 128};

    vk::PipelineLayoutCreateInfo createInfo{.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
                                            .pSetLayouts = descriptorSetLayouts.data(),
                                            .pushConstantRangeCount = 1,
                                            .pPushConstantRanges = &pushConstantRange};

    return device.createPipelineLayoutUnique(createInfo);
}

vk::ResultValue<vk::UniquePipeline> createComputePipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
                                                          const std::filesystem::path& shaderPath)
{
    const auto shaderCode = loadBinaryFile(shaderPath);

    vk::ShaderModuleCreateInfo shaderModuleInfo{.codeSize = shaderCode.size(),
                                                .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())};

    auto shaderModule = device.createShaderModuleUnique(shaderModuleInfo);
    if (shaderModule.result != vk::Result::eSuccess)
    {
        return {shaderModule.result, {}};
    }

    vk::PipelineShaderStageCreateInfo stageInfo{
        .stage = vk::ShaderStageFlagBits::eCompute, .module = *shaderModule.value, .pName = "main"};

    vk::ComputePipelineCreateInfo pipelineInfo{.stage = stageInfo, .layout = pipelineLayout};

    return device.createComputePipelineUnique({}, pipelineInfo);
}

vk::ResultValue<vk::UniquePipeline> createOccluderResolvePipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/occluder_resolve.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createVisibleDepthResolvePipeline(vk::Device device,
                                                                      vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/visible_depth_resolve.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createSceneUpdatePipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/scene_update.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createUniversalFrustumCullPipeline(vk::Device device,
                                                                       vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/universal_frustum_cull.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createLinearDepthPipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/linear_depth.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createHiZBuildPipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/hiz_build.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createGtaoPipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/gtao.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createGtaoDenoisePipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/gtao_denoise.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createOcclusionCull1Pipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/occlusion_cull_pass1.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createOcclusionCull2Pipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/occlusion_cull_pass2.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createPrefixScanPipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/prefix_scan.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createInstanceResolvePipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/instance_resolve.comp.spv");
}

vk::ResultValue<vk::UniquePipeline> createCascadeSetupPipeline(vk::Device device, vk::PipelineLayout pipelineLayout)
{

    return createComputePipeline(device, pipelineLayout, "../shaders/cascade_setup.comp.spv");
}

vk::ResultValue<vk::UniqueShaderModule> createShaderModule(vk::Device device, const std::filesystem::path& path)
{

    const auto code = loadBinaryFile(path);

    vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size(),
                                          .pCode = reinterpret_cast<const uint32_t*>(code.data())};

    return device.createShaderModuleUnique(createInfo);
}

vk::ResultValue<vk::UniquePipeline> createMainRenderPipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
                                                             vk::Format colorFormat, vk::Format depthFormat)
{
    auto vert = createShaderModule(device, "../shaders/main_pass.vert.spv");
    auto frag = createShaderModule(device, "../shaders/main_pass.frag.spv");

    std::array shaderStages = {vk::PipelineShaderStageCreateInfo{
                                   .stage = vk::ShaderStageFlagBits::eVertex, .module = *vert.value, .pName = "main"},
                               vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment,
                                                                 .module = *frag.value,
                                                                 .pName = "main"}};

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};

    vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
                                                        .rasterizerDiscardEnable = vk::False,
                                                        .polygonMode = vk::PolygonMode::eFill,
                                                        .cullMode = vk::CullModeFlagBits::eBack,
                                                        .frontFace = vk::FrontFace::eCounterClockwise,
                                                        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                       .sampleShadingEnable = vk::False};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True, .depthWriteEnable = vk::False, .depthCompareOp = vk::CompareOp::eLessOrEqual};

    vk::PipelineColorBlendAttachmentState blendAttachment{.blendEnable = vk::False};

    blendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                     vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlend{
        .logicOpEnable = vk::False, .attachmentCount = 1, .pAttachments = &blendAttachment};

    std::array dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                    .pDynamicStates = dynamicStates.data()};

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &colorFormat, .depthAttachmentFormat = depthFormat};

    return device.createGraphicsPipelineUnique({}, {.pNext = &renderingInfo,
                                                    .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                                    .pStages = shaderStages.data(),
                                                    .pVertexInputState = &vertexInput,
                                                    .pInputAssemblyState = &inputAssembly,
                                                    .pViewportState = &viewportState,
                                                    .pRasterizationState = &rasterizer,
                                                    .pMultisampleState = &multisample,
                                                    .pDepthStencilState = &depthStencil,
                                                    .pColorBlendState = &colorBlend,
                                                    .pDynamicState = &dynamicState,
                                                    .layout = pipelineLayout,
                                                    .renderPass = nullptr, // Using dynamic rendering
                                                    .subpass = 0});
}

vk::ResultValue<vk::UniquePipeline> createSkyboxPipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
                                                         vk::Format colorFormat, vk::Format depthFormat)
{
    auto vert = createShaderModule(device, "../shaders/skybox.vert.spv");
    auto frag = createShaderModule(device, "../shaders/skybox.frag.spv");

    std::array shaderStages = {vk::PipelineShaderStageCreateInfo{
                                   .stage = vk::ShaderStageFlagBits::eVertex, .module = *vert.value, .pName = "main"},
                               vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment,
                                                                 .module = *frag.value,
                                                                 .pName = "main"}};

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};

    vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = false,
                                                        .rasterizerDiscardEnable = false,
                                                        .polygonMode = vk::PolygonMode::eFill,
                                                        .cullMode = vk::CullModeFlagBits::eNone,
                                                        .frontFace = vk::FrontFace::eCounterClockwise,
                                                        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                       .sampleShadingEnable = vk::False};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = true, .depthWriteEnable = vk::False, .depthCompareOp = vk::CompareOp::eLessOrEqual};

    vk::PipelineColorBlendAttachmentState blendAttachment{.blendEnable = vk::False};

    blendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                     vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlend{
        .logicOpEnable = vk::False, .attachmentCount = 1, .pAttachments = &blendAttachment};

    std::array dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                    .pDynamicStates = dynamicStates.data()};

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &colorFormat, .depthAttachmentFormat = depthFormat};

    return device.createGraphicsPipelineUnique({}, {.pNext = &renderingInfo,
                                                    .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                                    .pStages = shaderStages.data(),
                                                    .pVertexInputState = &vertexInput,
                                                    .pInputAssemblyState = &inputAssembly,
                                                    .pViewportState = &viewportState,
                                                    .pRasterizationState = &rasterizer,
                                                    .pMultisampleState = &multisample,
                                                    .pDepthStencilState = &depthStencil,
                                                    .pColorBlendState = &colorBlend,
                                                    .pDynamicState = &dynamicState,
                                                    .layout = pipelineLayout,
                                                    .renderPass = nullptr, // Using dynamic rendering
                                                    .subpass = 0});
}

vk::ResultValue<vk::UniquePipeline> createShadowPassPipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
                                                             vk::Format depthFormat, uint32_t viewMask)
{

    auto vert = createShaderModule(device, "../shaders/shadow_pass.vert.spv");

    std::array shaderStages = {vk::PipelineShaderStageCreateInfo{
        .stage = vk::ShaderStageFlagBits::eVertex, .module = *vert.value, .pName = "main"}};

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};

    vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
                                                        .rasterizerDiscardEnable = vk::False,
                                                        .polygonMode = vk::PolygonMode::eFill,
                                                        .cullMode = vk::CullModeFlagBits::eFront,
                                                        .frontFace = vk::FrontFace::eCounterClockwise,
                                                        .depthBiasEnable = vk::True,
                                                        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                       .sampleShadingEnable = vk::False};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True, .depthWriteEnable = vk::True, .depthCompareOp = vk::CompareOp::eLessOrEqual};

    vk::PipelineColorBlendStateCreateInfo colorBlend{
        .logicOpEnable = vk::False, .attachmentCount = 0, .pAttachments = nullptr};

    std::array dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eDepthBias};

    vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                    .pDynamicStates = dynamicStates.data()};

    vk::PipelineRenderingCreateInfo renderingInfo{.viewMask = viewMask,
                                                  .colorAttachmentCount = 0,
                                                  .pColorAttachmentFormats = nullptr,
                                                  .depthAttachmentFormat = depthFormat};

    return device.createGraphicsPipelineUnique({}, {.pNext = &renderingInfo,
                                                    .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                                    .pStages = shaderStages.data(),
                                                    .pVertexInputState = &vertexInput,
                                                    .pInputAssemblyState = &inputAssembly,
                                                    .pViewportState = &viewportState,
                                                    .pRasterizationState = &rasterizer,
                                                    .pMultisampleState = &multisample,
                                                    .pDepthStencilState = &depthStencil,
                                                    .pColorBlendState = &colorBlend,
                                                    .pDynamicState = &dynamicState,
                                                    .layout = pipelineLayout,
                                                    .renderPass = nullptr, // Using dynamic rendering
                                                    .subpass = 0});
}

vk::ResultValue<vk::UniquePipeline> createOccluderPassPipeline(vk::Device device, vk::PipelineLayout pipelineLayout,
                                                               vk::Format depthFormat)
{

    auto vert = createShaderModule(device, "../shaders/occluder_pass.vert.spv");

    std::array shaderStages = {vk::PipelineShaderStageCreateInfo{
        .stage = vk::ShaderStageFlagBits::eVertex, .module = *vert.value, .pName = "main"}};

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};

    vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False,
                                                        .rasterizerDiscardEnable = vk::False,
                                                        .polygonMode = vk::PolygonMode::eFill,
                                                        .cullMode = vk::CullModeFlagBits::eFront,
                                                        .frontFace = vk::FrontFace::eCounterClockwise,
                                                        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                       .sampleShadingEnable = vk::False};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True, .depthWriteEnable = vk::True, .depthCompareOp = vk::CompareOp::eLess};

    vk::PipelineColorBlendStateCreateInfo colorBlend{
        .logicOpEnable = vk::False, .attachmentCount = 0, .pAttachments = nullptr};

    std::array dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eDepthBias};

    vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                    .pDynamicStates = dynamicStates.data()};

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount = 0, .pColorAttachmentFormats = nullptr, .depthAttachmentFormat = depthFormat};

    return device.createGraphicsPipelineUnique({}, {.pNext = &renderingInfo,
                                                    .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                                    .pStages = shaderStages.data(),
                                                    .pVertexInputState = &vertexInput,
                                                    .pInputAssemblyState = &inputAssembly,
                                                    .pViewportState = &viewportState,
                                                    .pRasterizationState = &rasterizer,
                                                    .pMultisampleState = &multisample,
                                                    .pDepthStencilState = &depthStencil,
                                                    .pColorBlendState = &colorBlend,
                                                    .pDynamicState = &dynamicState,
                                                    .layout = pipelineLayout,
                                                    .renderPass = nullptr, // Using dynamic rendering
                                                    .subpass = 0});
}

    vk::ResultValue<vk::UniquePipeline> createVisibleDepthPipeline(
    vk::Device device,
    vk::PipelineLayout pipelineLayout,
    vk::Format depthFormat)
{
    auto vertexShader =
        createShaderModule(
            device,
            "../shaders/occluder_pass.vert.spv");

    if (vertexShader.result != vk::Result::eSuccess)
    {
        return {vertexShader.result, {}};
    }

    vk::PipelineShaderStageCreateInfo shaderStages[]{
        {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *vertexShader.value,
            .pName = "main"
        }
    };

    vk::PipelineVertexInputStateCreateInfo vertexInput{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,

        // ДЛЯ ОТЛАДКИ
        .cullMode = vk::CullModeFlagBits::eNone,

        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples =
            vk::SampleCountFlagBits::e1
    };

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess
    };

    std::array dynamicStates{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates.data()
    };

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .stageCount = 1,
        .pStages = shaderStages,

        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pDynamicState = &dynamicState,

        .layout = pipelineLayout
    };

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr,
        .depthAttachmentFormat = depthFormat
    };

    pipelineInfo.pNext = &renderingInfo;

    auto [result, pipeline] =
        device.createGraphicsPipelineUnique(
            nullptr,
            pipelineInfo);

    return {result, std::move(pipeline)};
}

} // namespace

vk::ResultValue<DeviceRendererResources> createRendererResources(RenderContext& context, vk::Queue transferQueue,
                                                                 vk::CommandPool transferCommandPool)
{
    DeviceRendererResources resources{};

    // ============================================================
    // Descriptor Set Layouts
    // ============================================================

    auto [createRendererDescriptorSetLayoutResult, rendererSetLayout] =
        createRendererDescriptorSetLayout(context.device);
    if (createRendererDescriptorSetLayoutResult != vk::Result::eSuccess)
    {
        return {createRendererDescriptorSetLayoutResult, {}};
    }

    auto [createSceneDescriptorSetLayoutResult, sceneSetLayout] = createSceneDescriptorSetLayout(context.device);

    if (createSceneDescriptorSetLayoutResult != vk::Result::eSuccess)
    {
        return {createSceneDescriptorSetLayoutResult, {}};
    }

    auto [createEnvironmentDescriptorSetLayoutResult, envSetLayout] =
        createEnvironmentDescriptorSetLayout(context.device);

    if (createEnvironmentDescriptorSetLayoutResult != vk::Result::eSuccess)
    {
        return {createEnvironmentDescriptorSetLayoutResult, {}};
    }

    auto [createFrameDescriptorSetLayoutResult, frameSetLayout] = createFrameDescriptorSetLayout(context.device);

    if (createFrameDescriptorSetLayoutResult != vk::Result::eSuccess)
    {
        return {createFrameDescriptorSetLayoutResult, {}};
    }

    // ============================================================
    // Pipeline Layout
    // ============================================================

    auto [createPipelineLayoutResult, pipelineLayout] =
        createPipelineLayout(context.device, *rendererSetLayout, *envSetLayout, *sceneSetLayout, *frameSetLayout);

    if (createPipelineLayoutResult != vk::Result::eSuccess)
    {
        return {createPipelineLayoutResult, {}};
    }

    // ============================================================
    // Compute Pipelines
    // ============================================================

    auto [createOccluderResolvePipelineResult, occluderResolvePipeline] =
        createOccluderResolvePipeline(context.device, *pipelineLayout);
    if (createOccluderResolvePipelineResult != vk::Result::eSuccess)
    {
        return {createOccluderResolvePipelineResult, {}};
    }

    auto [createVisibleDepthResolvePipelineResult, visibleDepthResolvePipeline] =
        createVisibleDepthResolvePipeline(context.device, *pipelineLayout);

    if (createVisibleDepthResolvePipelineResult != vk::Result::eSuccess)
    {
        return {createVisibleDepthResolvePipelineResult, {}};
    }

    auto [createSceneUpdatePipelineResult, sceneUpdatePipeline] =
        createSceneUpdatePipeline(context.device, *pipelineLayout);

    if (createSceneUpdatePipelineResult != vk::Result::eSuccess)
    {
        return {createSceneUpdatePipelineResult, {}};
    }

    auto [createFrustumCullPipelineResult, frustumCullPipeline] =
        createUniversalFrustumCullPipeline(context.device, *pipelineLayout);

    if (createFrustumCullPipelineResult != vk::Result::eSuccess)
    {
        return {createFrustumCullPipelineResult, {}};
    }

    auto [createLinearDepthPipelineResult, linearDepthPipeline] =
        createLinearDepthPipeline(context.device, *pipelineLayout);

    if (createLinearDepthPipelineResult != vk::Result::eSuccess)
    {
        return {createLinearDepthPipelineResult, {}};
    }

    auto [createHizBuildPipelineResult, hizBuildPipeline] = createHiZBuildPipeline(context.device, *pipelineLayout);

    if (createHizBuildPipelineResult != vk::Result::eSuccess)
    {
        return {createHizBuildPipelineResult, {}};
    }

    auto [createGtaoPipelineResult, gtaoPipeline] = createGtaoPipeline(context.device, *pipelineLayout);

    if (createGtaoPipelineResult != vk::Result::eSuccess)
    {
        return {createGtaoPipelineResult, {}};
    }

    auto [createGtaoDenoisePipelineResult, gtaoDenoisePipeline] =
        createGtaoDenoisePipeline(context.device, *pipelineLayout);

    if (createGtaoDenoisePipelineResult != vk::Result::eSuccess)
    {
        return {createGtaoDenoisePipelineResult, {}};
    }

    auto [createOcclusionCull1PipelineResult, occlusionCull1Pipeline] =
        createOcclusionCull1Pipeline(context.device, *pipelineLayout);

    if (createOcclusionCull1PipelineResult != vk::Result::eSuccess)
    {
        return {createOcclusionCull1PipelineResult, {}};
    }

    auto [createOcclusionCull2PipelineResult, occlusionCull2Pipeline] =
        createOcclusionCull2Pipeline(context.device, *pipelineLayout);

    if (createOcclusionCull2PipelineResult != vk::Result::eSuccess)
    {
        return {createOcclusionCull2PipelineResult, {}};
    }

    auto [createPrefixScanPipelineResult, prefixScanPipeline] =
        createPrefixScanPipeline(context.device, *pipelineLayout);

    if (createPrefixScanPipelineResult != vk::Result::eSuccess)
    {
        return {createPrefixScanPipelineResult, {}};
    }

    auto [createInstanceResolvePipelineResult, instanceResolvePipeline] =
        createInstanceResolvePipeline(context.device, *pipelineLayout);

    if (createInstanceResolvePipelineResult != vk::Result::eSuccess)
    {
        return {createInstanceResolvePipelineResult, {}};
    }

    auto [createCascadeSetupPipelineResult, cascadeSetupPipeline] =
        createCascadeSetupPipeline(context.device, *pipelineLayout);

    if (createCascadeSetupPipelineResult != vk::Result::eSuccess)
    {
        return {createCascadeSetupPipelineResult, {}};
    }

    // ============================================================
    // Graphics Pipelines
    // ============================================================

    auto [createMainPipelineResult, mainPipeline] = createMainRenderPipeline(
        context.device, *pipelineLayout, context.swapchainColorFormat, context.swapchainDepthFormat);

    if (createMainPipelineResult != vk::Result::eSuccess)
    {
        return {createMainPipelineResult, {}};
    }

    auto [createSkyboxPipelineResult, skyboxPipeline] = createSkyboxPipeline(
        context.device, *pipelineLayout, context.swapchainColorFormat, context.swapchainDepthFormat);

    if (createSkyboxPipelineResult != vk::Result::eSuccess)
    {
        return {createSkyboxPipelineResult, {}};
    }

    auto [createShadowPipelineResult, shadowPipeline] =
        createShadowPassPipeline(context.device, *pipelineLayout, context.swapchainDepthFormat, 0b1111);

    if (createShadowPipelineResult != vk::Result::eSuccess)
    {
        return {createShadowPipelineResult, {}};
    }

    auto [createOccluderPipelineResult, occluderPipeline] =
        createOccluderPassPipeline(context.device, *pipelineLayout, context.swapchainDepthFormat);

    if (createOccluderPipelineResult != vk::Result::eSuccess)
    {
        return {createOccluderPipelineResult, {}};
    }

    auto [createVisibleDepthPipelineResult, visibleDepthPipeline] =
        createVisibleDepthPipeline(context.device, *pipelineLayout, context.swapchainDepthFormat);

    if (createVisibleDepthPipelineResult != vk::Result::eSuccess)
    {
        return {createVisibleDepthPipelineResult, {}};
    }

    // ============================================================
    // Samplers
    // ============================================================

    auto materialSamplerResult =
        context.device.createSamplerUnique(vk::SamplerCreateInfo{.magFilter = vk::Filter::eLinear,
                                                                 .minFilter = vk::Filter::eLinear,
                                                                 .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                                                 .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                                 .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                                 .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                                 .mipLodBias = 0.0f,
                                                                 .anisotropyEnable = vk::True,
                                                                 .maxAnisotropy = 16.0f,
                                                                 .compareEnable = vk::False,
                                                                 .compareOp = vk::CompareOp::eAlways,
                                                                 .minLod = 0.0f,
                                                                 .maxLod = vk::LodClampNone,
                                                                 .borderColor = vk::BorderColor::eIntOpaqueBlack,
                                                                 .unnormalizedCoordinates = vk::False});

    if (materialSamplerResult.result != vk::Result::eSuccess)
    {
        return {materialSamplerResult.result, {}};
    }

    auto nearestSamplerResult =
        context.device.createSamplerUnique(vk::SamplerCreateInfo{.magFilter = vk::Filter::eNearest,
                                                                 .minFilter = vk::Filter::eNearest,
                                                                 .mipmapMode = vk::SamplerMipmapMode::eNearest,
                                                                 .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                                                                 .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                                                                 .addressModeW = vk::SamplerAddressMode::eClampToEdge,
                                                                 .mipLodBias = 0.0f,
                                                                 .anisotropyEnable = vk::False,
                                                                 .maxAnisotropy = 1.0f,
                                                                 .compareEnable = vk::False,
                                                                 .compareOp = vk::CompareOp::eAlways,
                                                                 .minLod = 0.0f,
                                                                 .maxLod = VK_LOD_CLAMP_NONE,
                                                                 .borderColor = vk::BorderColor::eIntOpaqueBlack,
                                                                 .unnormalizedCoordinates = vk::False});

    if (nearestSamplerResult.result != vk::Result::eSuccess)
    {
        return {nearestSamplerResult.result, {}};
    }

    auto shadowSamplerResult =
        context.device.createSamplerUnique(vk::SamplerCreateInfo{.magFilter = vk::Filter::eLinear,
                                                                 .minFilter = vk::Filter::eLinear,
                                                                 .mipmapMode = vk::SamplerMipmapMode::eNearest,
                                                                 .addressModeU = vk::SamplerAddressMode::eClampToBorder,
                                                                 .addressModeV = vk::SamplerAddressMode::eClampToBorder,
                                                                 .addressModeW = vk::SamplerAddressMode::eClampToBorder,
                                                                 .mipLodBias = 0.0f,
                                                                 .anisotropyEnable = vk::False,
                                                                 .maxAnisotropy = 1.0f,
                                                                 .compareEnable = vk::False,
                                                                 .compareOp = vk::CompareOp::eAlways,
                                                                 .minLod = 0.0f,
                                                                 .maxLod = 0.0f,
                                                                 .borderColor = vk::BorderColor::eFloatOpaqueWhite,
                                                                 .unnormalizedCoordinates = vk::False});

    if (shadowSamplerResult.result != vk::Result::eSuccess)
    {
        return {shadowSamplerResult.result, {}};
    }

    // ============================================================
    // Store Layouts / Pipeline Layout / Pipelines / Samplers
    // ============================================================

    resources.rendererSetLayout = std::move(rendererSetLayout);

    resources.sceneSetLayout = std::move(sceneSetLayout);

    resources.environmentSetLayout = std::move(envSetLayout);

    resources.frameSetLayout = std::move(frameSetLayout);

    resources.pipelineLayout = std::move(pipelineLayout);

    resources.sceneUpdatePipeline = std::move(sceneUpdatePipeline);

    resources.occluderResolvePipeline = std::move(occluderResolvePipeline);

    resources.visibleDepthResolvePipeline = std::move(visibleDepthResolvePipeline);

    resources.universalFrustumCullPipeline = std::move(frustumCullPipeline);

    resources.linearDepthPipeline = std::move(linearDepthPipeline);

    resources.hizBuildPipeline = std::move(hizBuildPipeline);

    resources.gtaoPipeline = std::move(gtaoPipeline);

    resources.gtaoDenoisePipeline = std::move(gtaoDenoisePipeline);

    resources.occlusionCullPass1Pipeline = std::move(occlusionCull1Pipeline);

    resources.occlusionCullPass2Pipeline = std::move(occlusionCull2Pipeline);

    resources.prefixScanPipeline = std::move(prefixScanPipeline);

    resources.instanceResolvePipeline = std::move(instanceResolvePipeline);

    resources.cascadeSetupPipeline = std::move(cascadeSetupPipeline);

    resources.mainPipeline = std::move(mainPipeline);

    resources.skyboxPipeline = std::move(skyboxPipeline);

    resources.shadowPipeline = std::move(shadowPipeline);

    resources.occluderPipeline = std::move(occluderPipeline);

    resources.visibleDepthPipeline = std::move(visibleDepthPipeline);

    resources.materialSampler = std::move(materialSamplerResult.value);

    resources.nearestSampler = std::move(nearestSamplerResult.value);

    resources.shadowSampler = std::move(shadowSamplerResult.value);

    // ============================================================
    // Builtin Images: BRDF LUT + Fallback Textures
    // ============================================================

    if (auto result = createBuiltinRendererImages(context.device, transferQueue, transferCommandPool, context.allocator,
                                                  resources);
        result != vk::Result::eSuccess)
    {
        return {result, {}};
    }

    // ============================================================
    // Allocate Renderer Descriptor Set
    // ============================================================

    vk::DescriptorSetAllocateInfo allocateInfo{.descriptorPool = *context.descriptorPool,
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &*resources.rendererSetLayout};

    auto [allocateRendererSetResult, descriptorSets] = context.device.allocateDescriptorSets(allocateInfo);

    if (allocateRendererSetResult != vk::Result::eSuccess)
    {
        return {allocateRendererSetResult, {}};
    }

    resources.rendererSet = descriptorSets.front();

    // ============================================================
    // Update Renderer Descriptor Set
    // ============================================================

    vk::DescriptorImageInfo materialSamplerInfo{.sampler = *resources.materialSampler};

    vk::DescriptorImageInfo shadowSamplerInfo{.sampler = *resources.shadowSampler};

    vk::DescriptorImageInfo nearestSamplerInfo{.sampler = *resources.nearestSampler};

    vk::DescriptorImageInfo brdfLutImageInfo{.sampler = nullptr,
                                             .imageView = *resources.brdfLutImageView,
                                             .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

    std::array writes{vk::WriteDescriptorSet{.dstSet = resources.rendererSet,
                                             .dstBinding = static_cast<uint32_t>(RendererBindings::eMaterialSampler),
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = vk::DescriptorType::eSampler,
                                             .pImageInfo = &materialSamplerInfo},

                      vk::WriteDescriptorSet{.dstSet = resources.rendererSet,
                                             .dstBinding = static_cast<uint32_t>(RendererBindings::eShadowSampler),
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = vk::DescriptorType::eSampler,
                                             .pImageInfo = &shadowSamplerInfo},

                      vk::WriteDescriptorSet{.dstSet = resources.rendererSet,
                                             .dstBinding = static_cast<uint32_t>(RendererBindings::eNearestSampler),
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = vk::DescriptorType::eSampler,
                                             .pImageInfo = &nearestSamplerInfo},

                      vk::WriteDescriptorSet{.dstSet = resources.rendererSet,
                                             .dstBinding = static_cast<uint32_t>(RendererBindings::eBrdfLutImage),
                                             .dstArrayElement = 0,
                                             .descriptorCount = 1,
                                             .descriptorType = vk::DescriptorType::eSampledImage,
                                             .pImageInfo = &brdfLutImageInfo}};

    context.device.updateDescriptorSets(writes, {});

    return {vk::Result::eSuccess, std::move(resources)};
}

} // namespace shuttle::engine::render
