#pragma once

#include "IncludeVulkan.hpp"
#include "DeviceAllocator/VirtualBlock.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace shuttle::engine::render
{
    inline constexpr uint32_t InvalidDescriptorIndex =
        std::numeric_limits<uint32_t>::max();

    enum class DescriptorHeapBinding : uint32_t
    {
        Sampler = 0,

        Texture = 1,

        StorageImage = 2,

        UniformTexelBuffer = 3,
        StorageTexelBuffer = 4
    };

    class DescriptorHeapSet;

    struct DescriptorSlot
    {
        DescriptorHeapBinding binding{};
        uint32_t index{InvalidDescriptorIndex};

        [[nodiscard]]
        explicit operator bool() const noexcept
        {
            return index != InvalidDescriptorIndex;
        }
    };

    class UniqueDescriptorSlot
    {
    public:
        UniqueDescriptorSlot() = default;

        UniqueDescriptorSlot(
            DescriptorHeapSet& descriptorHeap,
            DescriptorHeapBinding binding,
            uint32_t index) noexcept;

        UniqueDescriptorSlot(UniqueDescriptorSlot const&) = delete;
        UniqueDescriptorSlot& operator=(UniqueDescriptorSlot const&) = delete;

        UniqueDescriptorSlot(UniqueDescriptorSlot&& other) noexcept;
        UniqueDescriptorSlot& operator=(UniqueDescriptorSlot&& other) noexcept;

        ~UniqueDescriptorSlot();

        void reset() noexcept;

        [[nodiscard]] DescriptorSlot release() noexcept;

        [[nodiscard]] uint32_t get() const noexcept
        {
            return index;
        }

        [[nodiscard]] DescriptorHeapBinding getBinding() const noexcept
        {
            return binding;
        }

        [[nodiscard]]
        explicit operator bool() const noexcept
        {
            return descriptorHeap != nullptr &&
                   index != InvalidDescriptorIndex;
        }

    private:
        DescriptorHeapSet* descriptorHeap{};
        DescriptorHeapBinding binding{};
        uint32_t index{InvalidDescriptorIndex};
    };

    struct DescriptorHeapSetCreateInfo
    {
        uint32_t textureCount = 4096;

        uint32_t samplerCount = 64;

        uint32_t storageImageCount = 1024;

        uint32_t uniformTexelBufferCount = 512;
        uint32_t storageTexelBufferCount = 512;

        vk::ShaderStageFlags stageFlags =
            vk::ShaderStageFlagBits::eVertex |
            vk::ShaderStageFlagBits::eFragment |
            vk::ShaderStageFlagBits::eCompute;
    };

    class DescriptorHeapSet
    {
    public:
        [[nodiscard]]
        static vk::ResultValue<DescriptorHeapSet> create(
            vk::Device device,
            DescriptorHeapSetCreateInfo const& createInfo);

        // ============================================================
        // Raw descriptor writes
        // ============================================================

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeTexture(
            vk::ImageView imageView,
            vk::ImageLayout imageLayout =
                vk::ImageLayout::eShaderReadOnlyOptimal);

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeSampler(
            vk::Sampler sampler);

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeStorageImage(
            vk::ImageView imageView,
            vk::ImageLayout imageLayout =
                vk::ImageLayout::eGeneral);

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeUniformTexelBuffer(
            vk::BufferView bufferView);

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeStorageTexelBuffer(
            vk::BufferView bufferView);

        // ============================================================
        // RAII descriptor writes
        // ============================================================

        [[nodiscard]]
        vk::ResultValue<UniqueDescriptorSlot> writeTextureUnique(
            vk::ImageView imageView,
            vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal);

        [[nodiscard]]
        vk::ResultValue<UniqueDescriptorSlot> writeSamplerUnique(
            vk::Sampler sampler);

        [[nodiscard]]
        vk::ResultValue<UniqueDescriptorSlot> writeStorageImageUnique(
            vk::ImageView imageView,
            vk::ImageLayout imageLayout =
                vk::ImageLayout::eGeneral);

        [[nodiscard]]
        vk::ResultValue<UniqueDescriptorSlot> writeUniformTexelBufferUnique(
            vk::BufferView bufferView);

        [[nodiscard]]
        vk::ResultValue<UniqueDescriptorSlot> writeStorageTexelBufferUnique(
            vk::BufferView bufferView);

        // ============================================================
        // Free
        // ============================================================

        void free(
            DescriptorHeapBinding binding,
            uint32_t index) noexcept;

        void freeTexture(uint32_t index) noexcept;

        void freeSampler(uint32_t index) noexcept;

        void freeStorageImage(uint32_t index) noexcept;

        void freeUniformTexelBuffer(uint32_t index) noexcept;
        void freeStorageTexelBuffer(uint32_t index) noexcept;

        [[nodiscard]]
        vk::DescriptorSet getDescriptorSet() const noexcept
        {
            return descriptorSet;
        }

        [[nodiscard]]
        vk::DescriptorSetLayout getDescriptorSetLayout() const noexcept
        {
            return *descriptorSetLayout;
        }

        DescriptorHeapSet() = default;

    private:
        struct Slot
        {
            resources::VirtualAllocation allocation{};
            bool occupied{};
        };

        struct BindingStorage
        {
            resources::UniqueVirtualBlock allocator{};
            std::vector<Slot> slots{};
            uint32_t capacity{};
        };

        DescriptorHeapSet(
            vk::Device device,
            vk::UniqueDescriptorPool descriptorPool,
            vk::UniqueDescriptorSetLayout descriptorSetLayout,
            vk::DescriptorSet descriptorSet,
            BindingStorage texture,
            BindingStorage sampler,
            BindingStorage storageImage,
            BindingStorage uniformTexelBuffer,
            BindingStorage storageTexelBuffer);

        [[nodiscard]]
        static vk::ResultValue<BindingStorage> createBindingStorage(
            uint32_t capacity);

        [[nodiscard]]
        static vk::ResultValue<uint32_t> allocateSlot(
            BindingStorage& storage) noexcept;

        static void freeSlot(
            BindingStorage& storage,
            uint32_t index) noexcept;

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeSampledImage(
            BindingStorage& storage,
            DescriptorHeapBinding binding,
            vk::ImageView imageView,
            vk::ImageLayout imageLayout) const;

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeImage(
            BindingStorage& storage,
            DescriptorHeapBinding binding,
            vk::DescriptorType descriptorType,
            vk::ImageView imageView,
            vk::ImageLayout imageLayout) const;

        [[nodiscard]]
        vk::ResultValue<uint32_t> writeTexelBuffer(
            BindingStorage& storage,
            DescriptorHeapBinding binding,
            vk::DescriptorType descriptorType,
            vk::BufferView bufferView) const;

        [[nodiscard]]
        vk::ResultValue<UniqueDescriptorSlot> makeUniqueSlot(
            vk::Result result,
            DescriptorHeapBinding binding,
            uint32_t index);

        vk::Device device{};

        vk::UniqueDescriptorPool descriptorPool{};
        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
        vk::DescriptorSet descriptorSet{};

        BindingStorage texture{};

        BindingStorage sampler{};

        BindingStorage storageImage{};

        BindingStorage uniformTexelBuffer{};
        BindingStorage storageTexelBuffer{};
    };
}