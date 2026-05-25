#pragma once
#include <map>
#include "IncludeVulkan.hpp"
#include <vector>

namespace vma {
	enum class AllocationCreateFlagBits : uint32_t {
		eDedicatedMemory = 0x00000001,
		eNeverAllocate = 0x00000002,
		eMapped = 0x00000004,
		eUserDataCopyString = 0x00000020,
		eUpperAddressBit = 0x00000040,
		eDontBind = 0x00000080,
		eWithinBud = 0x00000100,
		eCanAlias = 0x00000200,
		eHostAccessSequentialWrite = 0x00000400,
		eHostAccessRandom = 0x00000800,
		eStrategyMinMemory = 0x00010000,
		eCreateStrategyMinTime = 0x00020000,
		eCreateStrategyMinOffset = 0x00040000,
		eStrategyBestFit = eStrategyMinMemory,
		eStrategyFirestFit = eCreateStrategyMinTime
	};

	using AllocationCreateFlags = vk::Flags<AllocationCreateFlagBits>;

	enum class MemoryUsage {
		eUnknown,
		eGpuOnly,
		eCpuOnly,
		eCpuToGpu,
		eGpuToCpu,
		eAuto,
		eAutoPreferDedicatedMemory,
		ePreferHostMemory,
		ePreferDeviceMemory
	};

	using AllocationHandle = void*;
	using AllocatorHandle = void*;
	class Allocator;

	template <typename TResource>
		requires std::same_as<TResource, vk::Buffer> || std::same_as<TResource, vk::Image>
	class AllocatedResource {
	public:
		AllocatedResource() = default;
		AllocatedResource(AllocationHandle const& allocation, TResource resourceHandle)
			: allocation{ allocation }, resourceHandle{ resourceHandle } {}

		[[nodiscard]] AllocationHandle getAllocation() const { return allocation; }

		bool operator==(AllocatedResource const& other) const {
			return allocation == other.allocation && resourceHandle == other.resourceHandle;
		}

		bool operator!=(AllocatedResource const& other) const {
			return !(*this == other);
		}

		operator TResource() const { return resourceHandle; }

	private:
		AllocationHandle allocation = nullptr;
		TResource resourceHandle = VK_NULL_HANDLE;
	};

	using AllocatedBuffer = AllocatedResource<vk::Buffer>;
	using AllocatedImage = AllocatedResource<vk::Image>;

	struct BufferWriteInfo {
		AllocatedBuffer dstBuffer;
		vk::DeviceSize dstBufferOffset{};
		void const* srcData{};
		size_t dataSize{};
	};

	struct BufferReadInfo {
		AllocatedBuffer srcBuffer;
		vk::DeviceSize srcBufferOffset{};
		void* dstData{};
		size_t dataSize{};
	};

	class UniqueAllocatedBufferDeleter {
	public:
		UniqueAllocatedBufferDeleter(Allocator const& allocator) : allocator{ &allocator } {}
		UniqueAllocatedBufferDeleter() = default;

		void operator()(AllocatedBuffer const& allocatedBuffer) const;
	private:
		Allocator const* allocator = nullptr;
	};

	class UniqueAllocatedImageDeleter {
	public:
		UniqueAllocatedImageDeleter(Allocator const& allocator) : allocator{ &allocator } {}
		UniqueAllocatedImageDeleter() = default;

		void operator()(AllocatedImage const& allocatedImage) const;
	private:
		Allocator const* allocator = nullptr;
	};

	template <typename TResource, typename TDeleter>
		requires std::same_as<TResource, vk::Buffer> || std::same_as<TResource, vk::Image>
	class UniqueAllocatedResource {
	public:
		UniqueAllocatedResource() = default;
		UniqueAllocatedResource(AllocatedResource<TResource> allocatedResource, TDeleter deleter)
			: allocatedResource{ allocatedResource }, deleter{ deleter } {}

		UniqueAllocatedResource(UniqueAllocatedResource const&) = delete;
		UniqueAllocatedResource& operator=(UniqueAllocatedResource const&) = delete;

		UniqueAllocatedResource(UniqueAllocatedResource&& other) noexcept : allocatedResource{ other.allocatedResource }, deleter{ other.deleter } {
			other.allocatedResource = AllocatedResource<TResource>{};
			other.deleter = TDeleter{};
		}
		UniqueAllocatedResource& operator=(UniqueAllocatedResource&& other) noexcept {
			allocatedResource = other.allocatedResource;
			deleter = other.deleter;
			other.deleter = TDeleter{};
			other.allocatedResource = AllocatedResource<TResource>{};
			return *this;
		}

		AllocatedResource<TResource> const& get() const { return allocatedResource; }
		AllocatedResource<TResource>& get() { return allocatedResource; }

		AllocatedResource<TResource>* operator->() { return &allocatedResource; }
		AllocatedResource<TResource> const* operator->() const { return &allocatedResource; }

		AllocatedResource<TResource>& operator*() { return allocatedResource; }
		AllocatedResource<TResource> const& operator*() const { return allocatedResource; }

		~UniqueAllocatedResource() {
			if (allocatedResource != AllocatedResource<TResource>{})
			deleter(allocatedResource);
		}

	private:
		AllocatedResource<TResource> allocatedResource = AllocatedResource<TResource>{};
		TDeleter deleter{};
	};

	using UniqueAllocatedBuffer = UniqueAllocatedResource<vk::Buffer, UniqueAllocatedBufferDeleter>;
	using UniqueAllocatedImage = UniqueAllocatedResource<vk::Image, UniqueAllocatedImageDeleter>;

	class Allocator {
	public:
		Allocator() = default;
		Allocator(AllocatorHandle handle) : handle{ handle } {}

		bool operator==(Allocator const& other) const { return handle == other.handle; }

		[[nodiscard]] static vk::ResultValue<Allocator> create(
			vk::Instance instance, vk::Device device,
			vk::PhysicalDevice physicalDevice,
			vk::detail::DispatchLoaderDynamic const& dispatcher = vk::detail::defaultDispatchLoaderDynamic);

		[[nodiscard]] vk::ResultValue<AllocatedBuffer> createAndAllocateBuffer(
			vk::BufferCreateInfo const& bufferCreateInfo,
			MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
			AllocationCreateFlags allocationCreateFlags = {}) const;

		[[nodiscard]] vk::ResultValue<AllocatedImage> createAndAllocateImage(
			vk::ImageCreateInfo const& imageCreateInfo,
			MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
			AllocationCreateFlags allocationCreateFlags = {}) const;

		[[nodiscard]] vk::ResultValue<UniqueAllocatedBuffer> createAndAllocateBufferUnique(
			vk::BufferCreateInfo const& bufferCreateInfo,
			MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
			AllocationCreateFlags allocationCreateFlags = {}) const;

		[[nodiscard]] vk::ResultValue<UniqueAllocatedImage> createAndAllocateImageUnique(
			vk::ImageCreateInfo const& imageCreateInfo,
			MemoryUsage desireMemoryUsage = MemoryUsage::eAuto,
			AllocationCreateFlags allocationCreateFlags = {}) const;

		[[deprecated]] [[nodiscard]] vk::Result writeBufferFromHost(BufferWriteInfo const& writeInfo) const;
		[[deprecated]] [[nodiscard]] vk::Result readBufferToHost(BufferReadInfo const& readInfos) const;

		Allocator& operator=(Allocator const& other) = default;

		[[nodiscard]] vk::ResultValue<void*> mapMemory(AllocatedBuffer buffer) const;
		void unmapMemory(AllocatedBuffer buffer) const;

		void destroyBuffer(
			AllocatedBuffer allocatedBuffer) const;

		void destroyImage(
			AllocatedImage allocatedImage) const;

		void destroy() const;

	private:
		AllocatorHandle handle = nullptr;
	};

	class AllocatorCopier {
	public:
		AllocatorCopier(Allocator const& allocator) : allocator{ allocator } {}

		[[nodiscard, deprecated]] vk::Result writeBufferFromHost(BufferWriteInfo const& writeInfo) const {
			return allocator.writeBufferFromHost(writeInfo);
		}
		[[nodiscard, deprecated]] vk::Result readBufferToHost(BufferReadInfo const& readInfos) const {
			return allocator.readBufferToHost(readInfos);
		}

	private:
		Allocator allocator;
	};

	class UniqueAllocator {
	public:
		UniqueAllocator(Allocator const& allocator) : allocator{ allocator } {}

		UniqueAllocator() = default;
		[[nodiscard]] static vk::ResultValue<UniqueAllocator> makeUnique(
			vk::Instance instance, vk::Device device, 
			vk::PhysicalDevice physicalDevice, 
			vk::detail::DispatchLoaderDynamic const& dispatcher = vk::detail::defaultDispatchLoaderDynamic);

		UniqueAllocator(UniqueAllocator const&) = delete;
		UniqueAllocator& operator=(UniqueAllocator const&) = delete;
		UniqueAllocator(UniqueAllocator&& other) noexcept : allocator{ other.allocator } {
			other.allocator = Allocator{};
		}

		Allocator& get() { return allocator; }
		Allocator const& get() const { return allocator; }

		Allocator* operator->() { return &allocator; }
		Allocator const* operator->() const { return &allocator; }

		Allocator& operator*() { return allocator; }
		Allocator const& operator*() const { return allocator; }

		~UniqueAllocator() {
			if (allocator != Allocator{})
			allocator.destroy();
		}
	private:
		Allocator allocator;
	};
}

namespace vk {

	template <>
	// Specialization of flagTraits for vma::AllocationCreateFlagBits to define allFlags
	struct FlagTraits<vma::AllocationCreateFlagBits> {
		using WrappedType = uint32_t;
		static constexpr bool isBitmask = true;
		static constexpr vma::AllocationCreateFlags allFlags = vma::AllocationCreateFlagBits::eDedicatedMemory | vma::AllocationCreateFlagBits::eNeverAllocate | vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eUserDataCopyString |
			vma::AllocationCreateFlagBits::eUpperAddressBit | vma::AllocationCreateFlagBits::eDontBind | vma::AllocationCreateFlagBits::eWithinBud |
			vma::AllocationCreateFlagBits::eCanAlias | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eHostAccessRandom |
			vma::AllocationCreateFlagBits::eStrategyMinMemory | vma::AllocationCreateFlagBits::eCreateStrategyMinTime | vma::AllocationCreateFlagBits::eCreateStrategyMinOffset;
	};

}