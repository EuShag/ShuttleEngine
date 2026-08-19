#include "DescriptorHeapSet.hpp"

namespace shuttle::engine::render
{
    // ============================================================
    // UniqueDescriptorSlot
    // ============================================================

    UniqueDescriptorSlot::UniqueDescriptorSlot(
        DescriptorHeapSet& descriptorHeap,
        DescriptorHeapBinding binding,
        uint32_t index) noexcept
        :
        descriptorHeap(&descriptorHeap),
        binding(binding),
        index(index)
    {
    }

    UniqueDescriptorSlot::UniqueDescriptorSlot(
        UniqueDescriptorSlot&& other) noexcept
        :
        descriptorHeap(other.descriptorHeap),
        binding(other.binding),
        index(other.index)
    {
        other.descriptorHeap = nullptr;
        other.index = InvalidDescriptorIndex;
    }

    UniqueDescriptorSlot& UniqueDescriptorSlot::operator=(
        UniqueDescriptorSlot&& other) noexcept
    {
        if (this == &other) return *this;

        reset();

        descriptorHeap = other.descriptorHeap;
        binding = other.binding;
        index = other.index;

        other.descriptorHeap = nullptr;
        other.index = InvalidDescriptorIndex;

        return *this;
    }

    UniqueDescriptorSlot::~UniqueDescriptorSlot()
    {
        reset();
    }

    void UniqueDescriptorSlot::reset() noexcept
    {
        if (descriptorHeap == nullptr ||
            index == InvalidDescriptorIndex)
        {
            return;
        }

        descriptorHeap->free(binding, index);

        descriptorHeap = nullptr;
        index = InvalidDescriptorIndex;
    }

    DescriptorSlot UniqueDescriptorSlot::release() noexcept
    {
        DescriptorSlot released{
            .binding = binding,
            .index = index
        };

        descriptorHeap = nullptr;
        index = InvalidDescriptorIndex;

        return released;
    }

    // ============================================================
    // DescriptorHeapSet
    // ============================================================

    DescriptorHeapSet::DescriptorHeapSet(
        vk::Device device,
        vk::UniqueDescriptorPool descriptorPool,
        vk::UniqueDescriptorSetLayout descriptorSetLayout,
        vk::DescriptorSet descriptorSet,
        BindingStorage texture,
        BindingStorage sampler,
        BindingStorage storageImage,
        BindingStorage uniformTexelBuffer,
        BindingStorage storageTexelBuffer)
        :
        device(device),
        descriptorPool(std::move(descriptorPool)),
        descriptorSetLayout(std::move(descriptorSetLayout)),
        descriptorSet(descriptorSet),
        texture(std::move(texture)),
        sampler(std::move(sampler)),
        storageImage(std::move(storageImage)),
        uniformTexelBuffer(std::move(uniformTexelBuffer)),
        storageTexelBuffer(std::move(storageTexelBuffer))
    {
    }

    vk::ResultValue<DescriptorHeapSet::BindingStorage> DescriptorHeapSet::createBindingStorage(uint32_t capacity)
    {
        auto [createBlockResult, block] = resources::UniqueVirtualBlock::makeUnique(capacity);

        if (createBlockResult != vk::Result::eSuccess) return {createBlockResult, {}};

        BindingStorage storage{};
        storage.allocator = std::move(block);
        storage.slots.resize(capacity);
        storage.capacity = capacity;

        return {vk::Result::eSuccess, std::move(storage)};
    }

    vk::ResultValue<DescriptorHeapSet> DescriptorHeapSet::create(
        vk::Device device,
        DescriptorHeapSetCreateInfo const& createInfo)
    {
        // ============================================================
        // Descriptor Pool
        // ============================================================

        std::array poolSizes{
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampledImage,
                .descriptorCount = createInfo.textureCount
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampler,
                .descriptorCount = createInfo.samplerCount
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageImage,
                .descriptorCount = createInfo.storageImageCount
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eUniformTexelBuffer,
                .descriptorCount = createInfo.uniformTexelBufferCount
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageTexelBuffer,
                .descriptorCount = createInfo.storageTexelBufferCount
            }
        };

        auto [createPoolResult, descriptorPool] = device.createDescriptorPoolUnique( vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = 1,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        });

        if (createPoolResult != vk::Result::eSuccess) return {createPoolResult, {}};

        // ============================================================
        // Descriptor Set Layout
        // ============================================================

        std::array bindings{
            vk::DescriptorSetLayoutBinding{
                .binding = static_cast<uint32_t>(DescriptorHeapBinding::Texture),
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = createInfo.textureCount,
                .stageFlags = createInfo.stageFlags
            },
            vk::DescriptorSetLayoutBinding{
                .binding = static_cast<uint32_t>(DescriptorHeapBinding::Sampler),
                .descriptorType = vk::DescriptorType::eSampler,
                .descriptorCount = createInfo.samplerCount,
                .stageFlags = createInfo.stageFlags
            },
            vk::DescriptorSetLayoutBinding{
                .binding = static_cast<uint32_t>(DescriptorHeapBinding::StorageImage),
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = createInfo.storageImageCount,
                .stageFlags = createInfo.stageFlags
            },
            vk::DescriptorSetLayoutBinding{
                .binding = static_cast<uint32_t>(DescriptorHeapBinding::UniformTexelBuffer),
                .descriptorType = vk::DescriptorType::eUniformTexelBuffer,
                .descriptorCount = createInfo.uniformTexelBufferCount,
                .stageFlags = createInfo.stageFlags
            },
            vk::DescriptorSetLayoutBinding{
                .binding = static_cast<uint32_t>(DescriptorHeapBinding::StorageTexelBuffer),
                .descriptorType = vk::DescriptorType::eStorageTexelBuffer,
                .descriptorCount = createInfo.storageTexelBufferCount,
                .stageFlags = createInfo.stageFlags
            }
        };

        std::array bindingFlags{
            vk::DescriptorBindingFlags{
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind
            },
            vk::DescriptorBindingFlags{
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind
            },
            vk::DescriptorBindingFlags{
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind
            },
            vk::DescriptorBindingFlags{
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind
            },
            vk::DescriptorBindingFlags{
                vk::DescriptorBindingFlagBits::ePartiallyBound |
                vk::DescriptorBindingFlagBits::eUpdateAfterBind
            }
        };

        vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
            .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
            .pBindingFlags = bindingFlags.data()
        };

        auto [createLayoutResult, descriptorSetLayout] = device.createDescriptorSetLayoutUnique({
            .pNext = &bindingFlagsInfo,
            .flags =vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        });

        if (createLayoutResult != vk::Result::eSuccess) return {createLayoutResult, {}};

        // ============================================================
        // Descriptor Set
        // ============================================================

        vk::DescriptorSetLayout layout = *descriptorSetLayout;

        auto [allocateSetResult, descriptorSets] = device.allocateDescriptorSets({
            .descriptorPool = *descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout
        });

        if (allocateSetResult != vk::Result::eSuccess)
        {
            return {allocateSetResult, {}};
        }

        // ============================================================
        // Slot Allocators
        // ============================================================

        auto [createTextureResult, texture] = createBindingStorage(createInfo.textureCount);

        if (createTextureResult != vk::Result::eSuccess)
        {
            return {createTextureResult, {}};
        }

        auto [createSamplerResult, sampler] = createBindingStorage(createInfo.samplerCount);

        if (createSamplerResult != vk::Result::eSuccess)
        {
            return {createSamplerResult,{}};
        }

        auto [createStorageImageResult, storageImage] =
            createBindingStorage(createInfo.storageImageCount);

        if (createStorageImageResult != vk::Result::eSuccess)
        {
            return {createStorageImageResult,{}};
        }

        auto [createUniformTexelBufferResult, uniformTexelBuffer] =
            createBindingStorage(createInfo.uniformTexelBufferCount);

        if (createUniformTexelBufferResult != vk::Result::eSuccess)
        {
            return {createUniformTexelBufferResult, {}};
        }

        auto [createStorageTexelBufferResult, storageTexelBuffer] = createBindingStorage(createInfo.storageTexelBufferCount);

        if (createStorageTexelBufferResult != vk::Result::eSuccess)
        {
            return {createStorageTexelBufferResult, {}};
        }

        return {
            vk::Result::eSuccess,
            DescriptorHeapSet{
                device,
                std::move(descriptorPool),
                std::move(descriptorSetLayout),
                descriptorSets.front(),
                std::move(texture),
                std::move(sampler),
                std::move(storageImage),
                std::move(uniformTexelBuffer),
                std::move(storageTexelBuffer)
            }
        };
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::allocateSlot(BindingStorage& storage) noexcept
    {
        auto [allocateResult, allocation] = storage.allocator->allocate(1, 1);

        if (allocateResult != vk::Result::eSuccess) return {allocateResult, 0};

        auto index = static_cast<uint32_t>(allocation.offset);

        if (index >= storage.capacity)
        {
            storage.allocator->free(allocation);
            return {vk::Result::eErrorOutOfDeviceMemory, 0};
        }

        storage.slots[index] = Slot{ .allocation = allocation, .occupied = true };
        return {vk::Result::eSuccess, index};
    }

    void DescriptorHeapSet::freeSlot(BindingStorage& storage, uint32_t index) noexcept
    {
        if (index >= storage.capacity) return;
        Slot& slot = storage.slots[index];
        if (!slot.occupied) return;
        storage.allocator->free(slot.allocation);
        slot = Slot{};
    }

    void DescriptorHeapSet::free(DescriptorHeapBinding binding, uint32_t index) noexcept
    {
        switch (binding)
        {
        case DescriptorHeapBinding::Texture:
            freeTexture(index);
            break;

        case DescriptorHeapBinding::Sampler:
            freeSampler(index);
            break;

        case DescriptorHeapBinding::StorageImage:
            freeStorageImage(index);
            break;

        case DescriptorHeapBinding::UniformTexelBuffer:
            freeUniformTexelBuffer(index);
            break;

        case DescriptorHeapBinding::StorageTexelBuffer:
            freeStorageTexelBuffer(index);
            break;

        default:
            break;
        }
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeImage(
        BindingStorage& storage,
        DescriptorHeapBinding binding,
        vk::DescriptorType descriptorType,
        vk::ImageView imageView,
        vk::ImageLayout imageLayout) const
    {
        auto [allocateResult, index] = allocateSlot(storage);

        if (allocateResult != vk::Result::eSuccess) return {allocateResult, 0};

        vk::DescriptorImageInfo imageInfo{
            .sampler = nullptr,
            .imageView = imageView,
            .imageLayout = imageLayout
        };

        vk::WriteDescriptorSet write{
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(binding),
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = descriptorType,
            .pImageInfo = &imageInfo
        };

        device.updateDescriptorSets(1, &write, 0, nullptr);

        return {vk::Result::eSuccess, index};
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeSampledImage(
        BindingStorage& storage,
        DescriptorHeapBinding binding,
        vk::ImageView imageView,
        vk::ImageLayout imageLayout) const
    {
        return writeImage(
            storage,
            binding,
            vk::DescriptorType::eSampledImage,
            imageView,
            imageLayout);
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeTexelBuffer(
        BindingStorage& storage,
        DescriptorHeapBinding binding,
        vk::DescriptorType descriptorType,
        vk::BufferView bufferView) const
    {
        auto [allocateResult, index] = allocateSlot(storage);

        if (allocateResult != vk::Result::eSuccess) return {allocateResult, 0};


        vk::WriteDescriptorSet write{
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(binding),
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = descriptorType,
            .pTexelBufferView = &bufferView
        };

        device.updateDescriptorSets(1, &write, 0, nullptr);

        return {vk::Result::eSuccess, index};
    }

    vk::ResultValue<UniqueDescriptorSlot> DescriptorHeapSet::makeUniqueSlot(vk::Result result, DescriptorHeapBinding binding, uint32_t index)
    {
        if (result != vk::Result::eSuccess) return {result, {}};
        return {vk::Result::eSuccess, UniqueDescriptorSlot{*this, binding, index}};
    }

    // ============================================================
    // Raw writes
    // ============================================================

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeTexture(vk::ImageView imageView,vk::ImageLayout imageLayout)
    {
        return writeSampledImage(
            texture,
            DescriptorHeapBinding::Texture,
            imageView,
            imageLayout);
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeSampler(vk::Sampler samplerHandle)
    {
        auto [allocateResult, index] = allocateSlot(sampler);

        if (allocateResult != vk::Result::eSuccess) return {allocateResult, 0};

        vk::DescriptorImageInfo samplerInfo{
            .sampler = samplerHandle,
            .imageView = nullptr,
            .imageLayout = vk::ImageLayout::eUndefined
        };

        vk::WriteDescriptorSet write{
            .dstSet = descriptorSet,
            .dstBinding = static_cast<uint32_t>(DescriptorHeapBinding::Sampler),
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .pImageInfo = &samplerInfo
        };

        device.updateDescriptorSets(1, &write, 0, nullptr);

        return {vk::Result::eSuccess,  index};
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeStorageImage(vk::ImageView imageView, vk::ImageLayout imageLayout)
    {
        return writeImage(
            storageImage,
            DescriptorHeapBinding::StorageImage,
            vk::DescriptorType::eStorageImage,
            imageView,
            imageLayout);
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeUniformTexelBuffer(vk::BufferView bufferView)
    {
        return writeTexelBuffer(
            uniformTexelBuffer,
            DescriptorHeapBinding::UniformTexelBuffer,
            vk::DescriptorType::eUniformTexelBuffer,
            bufferView);
    }

    vk::ResultValue<uint32_t> DescriptorHeapSet::writeStorageTexelBuffer(vk::BufferView bufferView)
    {
        return writeTexelBuffer(
            storageTexelBuffer,
            DescriptorHeapBinding::StorageTexelBuffer,
            vk::DescriptorType::eStorageTexelBuffer,
            bufferView);
    }

    // ============================================================
    // Unique writes
    // ============================================================

    vk::ResultValue<UniqueDescriptorSlot> DescriptorHeapSet::writeTextureUnique(vk::ImageView imageView, vk::ImageLayout imageLayout)
    {
        auto [result, index] = writeTexture(imageView, imageLayout);

        return makeUniqueSlot(result, DescriptorHeapBinding::Texture, index);
    }

    vk::ResultValue<UniqueDescriptorSlot> DescriptorHeapSet::writeSamplerUnique(vk::Sampler samplerHandle)
    {
        auto [result, index] = writeSampler(samplerHandle);

        return makeUniqueSlot(result, DescriptorHeapBinding::Sampler, index);
    }

    vk::ResultValue<UniqueDescriptorSlot> DescriptorHeapSet::writeStorageImageUnique(vk::ImageView imageView, vk::ImageLayout imageLayout)
    {
        auto [result, index] = writeStorageImage(imageView, imageLayout);

        return makeUniqueSlot(result, DescriptorHeapBinding::StorageImage, index);
    }

    vk::ResultValue<UniqueDescriptorSlot> DescriptorHeapSet::writeUniformTexelBufferUnique(vk::BufferView bufferView)
    {
        auto [result, index] = writeUniformTexelBuffer(bufferView);

        return makeUniqueSlot(result, DescriptorHeapBinding::UniformTexelBuffer, index);
    }

    vk::ResultValue<UniqueDescriptorSlot> DescriptorHeapSet::writeStorageTexelBufferUnique(vk::BufferView bufferView)
    {
        auto [result, index] = writeStorageTexelBuffer(bufferView);

        return makeUniqueSlot(result, DescriptorHeapBinding::StorageTexelBuffer, index);
    }

    // ============================================================
    // Free typed slots
    // ============================================================

    void DescriptorHeapSet::freeTexture(uint32_t index) noexcept
    {
        freeSlot(texture, index);
    }

    void DescriptorHeapSet::freeSampler(uint32_t index) noexcept
    {
        freeSlot(sampler, index);
    }

    void DescriptorHeapSet::freeStorageImage(uint32_t index) noexcept
    {
        freeSlot(storageImage, index);
    }

    void DescriptorHeapSet::freeUniformTexelBuffer(uint32_t index) noexcept
    {
        freeSlot(uniformTexelBuffer,index);
    }

    void DescriptorHeapSet::freeStorageTexelBuffer(uint32_t index) noexcept
    {
        freeSlot(storageTexelBuffer, index);
    }
}