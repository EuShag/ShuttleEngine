#pragma once
#include <map>
#include <optional>
#include <memory>
#include "IncludeVulkan.hpp"

template<typename T>
concept VulkanStructure = requires(T structure) {
	{ T::structureType } -> std::convertible_to<vk::StructureType>;
	{ structure.sType } -> std::convertible_to<vk::StructureType>;
};

class BaseStructureHolder {
public:
	virtual vk::BaseInStructure* getBaseInStructure() = 0;
	[[nodiscard]] virtual vk::StructureType getSType() const = 0;
	virtual ~BaseStructureHolder() = default;
};

template<VulkanStructure T>
class StructureHolder : public BaseStructureHolder {
public:
	explicit StructureHolder(T structure) : data{ structure } {}
	T& get() { return data; }
	T const& get() const { return data; }
	vk::BaseInStructure* getBaseInStructure() override { return reinterpret_cast<vk::BaseInStructure*>(&data); }
	[[nodiscard]] vk::StructureType getSType() const override { return T::structureType; }
private:
	T data;
};

class VulkanStructureChain {
public:
	template<VulkanStructure T>
	T& get() {
		if (!contains<T>()) {
			throw std::runtime_error{ "Structure of type " + std::to_string(static_cast<VkStructureType>(T::structureType)) + " not found in the chain." };
		}
		return static_cast<StructureHolder<T>*>(structureMap[T::structureType])->get();
	}
	template<VulkanStructure T>
	void add(T structure) {
		auto const lastIt = structures.empty() ? nullptr : structures.back().get();
		auto structureHolder = std::make_unique<StructureHolder<T>>(structure);
		structureMap[structure.sType] = structureHolder.get();
		structures.push_back(std::move(structureHolder));
		if (lastIt != nullptr) {
			lastIt->getBaseInStructure()->pNext = structureHolder->getBaseInStructure();
		}
	}

	template<VulkanStructure T>
	[[nodiscard]] bool contains() {
		return structureMap.contains(T::structureType);
	}

	[[nodiscard]] vk::BaseInStructure* getChainHead() const {
		return structures.empty() ? nullptr : structures.front()->getBaseInStructure();
	}
private:
	std::vector<std::unique_ptr<BaseStructureHolder>> structures;
	std::map<vk::StructureType, BaseStructureHolder*> structureMap;
};