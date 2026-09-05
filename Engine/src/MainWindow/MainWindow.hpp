/**
 * @file MainWindow.hpp
 * @brief Header file for the Shuttle Engine Editor Main Window.
 *
 * @license
 * Copyright (c) 2026 Shuttle Engine Project.
 * All rights reserved.
 *
 * This source code is licensed under the MIT License found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef SHUTTLEENGINE_MAINWINDOW_HPP
#define SHUTTLEENGINE_MAINWINDOW_HPP

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../../Assets/EnvironmentCompiler/include/Assets/EnvironmentCompiler/CpuIblGenerator.hpp"
#include "Assets/SceneCompiler/SceneCompiler.hpp"
#include "DeviceAllocator/DeviceAllocator.hpp"
#include "Render/MainPass.hpp"
#include "ImGuiContextM/ImGuiContextM.hpp"
#include "backends/imgui_impl_vulkan.h"

namespace pfd
{
    class save_file;
    class open_file;
} // namespace pfd

namespace shuttle::editor::core
{

    /**
     * @brief Unique identifier type for editor resources and assets.
     */
    using ResourceId = uint32_t;

    /**
     * @brief Enumeration of supported editor asset types.
     */
    enum class AssetType : uint32_t
    {
        Scene = 0,    /**< 3D Scene asset. */
        Environment  /**< Environment/IBL asset. */
    };

    // ============================================================
    // Resource Management Structures (Resource Bin)
    // ============================================================

    /**
     * @brief RAII wrapper for managing ImGui Vulkan viewport descriptor sets.
     * Automatically removes texture from ImGui when destroyed or overwritten.
     */
    struct UniqueViewportDescriptorSet
    {
        /**
         * @brief Constructs wrapper around an existing Vulkan descriptor set.
         * @param descriptorSet Vulkan descriptor set handle.
         */
        UniqueViewportDescriptorSet(VkDescriptorSet descriptorSet)
            : descriptorSet{descriptorSet}
        {
        }

        /**
         * @brief Default constructor creating an empty descriptor set handle.
         */
        UniqueViewportDescriptorSet() = default;

        /**
         * @brief Move constructor.
         * @param other Object to move from.
         */
        UniqueViewportDescriptorSet(UniqueViewportDescriptorSet&& other) noexcept
        {
            descriptorSet = other.descriptorSet;
            other.descriptorSet = VK_NULL_HANDLE;
        }

        /**
         * @brief Move assignment operator.
         * @param other Object to move from.
         * @return Reference to self.
         */
        UniqueViewportDescriptorSet& operator=(UniqueViewportDescriptorSet&& other) noexcept
        {
            if (this->descriptorSet != other.descriptorSet)
            {
                if (descriptorSet != VK_NULL_HANDLE)
                {
                    ImGui_ImplVulkan_RemoveTexture(descriptorSet);
                }
                descriptorSet = other.descriptorSet;
                other.descriptorSet = VK_NULL_HANDLE;
            }
            return *this;
        }

        /**
         * @brief Destructor releasing ImGui Vulkan texture resource.
         */
        ~UniqueViewportDescriptorSet()
        {
            if (descriptorSet != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_RemoveTexture(descriptorSet);
            }
        }

        /**
         * @brief Gets raw descriptor set handle.
         * @return VkDescriptorSet handle.
         */
        VkDescriptorSet operator*() const
        {
            return descriptorSet;
        }

    private:
        VkDescriptorSet descriptorSet{};
    };

    /**
     * @brief Deferred resource cleanup bin for retiring GPU resources safely across frame delays.
     */
    class ResourceBin
    {
    public:
        /**
         * @brief Retired image resource entry with associated view and asset identifier.
         */
        struct RetiredImage
        {
            resources::UniqueAllocatedImage image; /**< Allocated image handle. */
            vk::UniqueImageView view;             /**< Image view handle. */
            ResourceId id;                        /**< Associated resource ID. */
        };

        /**
         * @brief Retired descriptor set entry with associated asset identifier.
         */
        struct RetiredDescriptorSet
        {
            UniqueViewportDescriptorSet descriptorSet; /**< Unique descriptor set wrapper. */
            ResourceId id{};                           /**< Associated resource ID. */
        };

        /**
         * @brief Retires an allocated image and image view for deferred destruction.
         * @param img Image object to retire.
         * @param view Image view object to retire.
         * @param id Associated resource ID.
         */
        void retireImage(resources::UniqueAllocatedImage&& img, vk::UniqueImageView&& view, ResourceId id)
        {
            m_images.push_back({std::move(img), std::move(view), id});
        }

        /**
         * @brief Retires a viewport descriptor set for deferred destruction.
         * @param ds Descriptor set object to retire.
         * @param id Associated resource ID.
         */
        void retireDescriptorSet(UniqueViewportDescriptorSet&& ds, ResourceId id)
        {
            m_descriptorSets.push_back({std::move(ds), id});
        }

        /**
         * @brief Releases retired resources associated with given resource ID.
         * @param id Resource ID to release.
         */
        void release(ResourceId id);

        /**
         * @brief Clears all retired images and descriptor sets immediately.
         */
        void clear()
        {
            m_images.clear();
            m_descriptorSets.clear();
        }

    private:
        std::vector<RetiredImage> m_images;
        std::vector<RetiredDescriptorSet> m_descriptorSets;
    };

    // ============================================================
    // Asset Data Management
    // ============================================================

    /**
     * @brief Information container representing an asset loaded in the editor workspace.
     */
    struct LoadedAsset
    {
        std::string name;             /**< Display name of the asset. */
        std::filesystem::path path;   /**< Disk location of the asset file. */
        bool isDirty = false;         /**< Indicates unsaved changes. */
        ResourceId id = 0;            /**< Unique asset identifier. */
        AssetType type = AssetType::Scene; /**< Category of asset. */

        uint32_t meshCount = 0;       /**< Number of meshes (statistics). */
        uint32_t materialCount = 0;   /**< Number of materials (statistics). */

        bool isScene = true;          /**< True if asset is a scene, false if environment. */
    };

    /**
     * @brief Layout modes for viewport rendering windows.
     */
    enum class ViewportLayoutMode : uint32_t
    {
        Single = 0,     /**< Single full-size viewport panel. */
        SplitVertical,  /**< Two vertically split viewports. */
        SplitHorizontal,/**< Two horizontally split viewports. */
        Quad            /**< Four tiled quad viewports. */
    };

    /**
     * @brief Main Editor GUI Window class responsible for rendering dockspaces, toolbars, and viewports.
     */
    class MainWindow : public IUiPainter
    {
    public:
        /**
         * @brief Searches loaded assets list by ID.
         * @param id Asset ID to look up.
         * @return Pointer to LoadedAsset if found, nullptr otherwise.
         */
        LoadedAsset* findAsset(ResourceId id);

        /**
         * @brief Constructs MainWindow editor UI object.
         * @param window Pointer to host SDL window.
         * @param mainPassSettings Initial rendering pass configuration.
         * @param isMaximized Initial window maximization state.
         */
        MainWindow(shuttle::pal::WindowBase* window, engine::render::MainPassSettings const &mainPassSettings, bool isMaximized = false);

        /**
         * @brief Default constructor.
         */
        MainWindow() = default;

        /**
         * @brief Main ImGui UI rendering implementation override.
         */
        void drawUi() override;

        /**
         * @brief Polls asynchronous portable file dialog responses.
         */
        void pollFileDialogs();

        /**
         * @brief Renders overlay FPS and performance statistics counter.
         */
        static void drawFpsCounter();

        // Viewport API

        /**
         * @brief Retrieves current viewport resolution extent.
         * @return Viewport pixel extent as vk::Extent2D.
         */
        [[nodiscard]] vk::Extent2D getViewportExtent() const;

        /**
         * @brief Sets descriptor set for final composite viewport texture.
         * @param finalDescriptorSet ImGui Vulkan descriptor set handle.
         */
        void setFinalViewportImage(vk::DescriptorSet finalDescriptorSet);

        /**
         * @brief Sets descriptor sets for debug/attachment viewport outputs.
         * @param outputDescriptorSets Array of 4 debug layer descriptor sets.
         */
        void setDebugViewportImages(std::array<VkDescriptorSet, 4> const &outputDescriptorSets);

        // Callbacks

        /**
         * @brief Registers callback triggered when opening a scene file.
         * @param cb Callback function accepting scene file path.
         */
        void setOpenSceneCallback(std::function<void(std::filesystem::path const&)> const& cb) { openSceneCallback = cb; }

        /**
         * @brief Registers callback triggered when opening an environment file.
         * @param cb Callback function accepting environment file path.
         */
        void setOpenEnvironmentCallback(std::function<void(std::filesystem::path const&)> const& cb) { openEnvironmentCallback = cb; }

        /**
         * @brief Registers callback triggered when importing a raw scene asset with compiler options.
         * @param cb Callback function accepting file path and compiler options.
         */
        void setImportSceneCallback(std::function<void(std::filesystem::path const&, assets::scene_compiler::SceneCompilerOptions const&)> const& cb) { importSceneCallback = cb; }

        /**
         * @brief Registers callback triggered when importing an environment asset with IBL settings.
         * @param cb Callback function accepting file path and IBL settings.
         */
        void setImportEnvironmentCallback(std::function<void(std::filesystem::path const&, engine::ibl::IblGenerationSettings const&)> const& cb) { importEnvironmentCallback = cb; }

        /**
         * @brief Registers callback triggered when saving active scene file.
         * @param cb Callback function accepting target save file path.
         */
        void setSaveSceneCallback(std::function<void(std::filesystem::path const&)> const& cb) { saveSceneCallback = cb; }

        /**
         * @brief Registers callback triggered when saving active environment file.
         * @param cb Callback function accepting target save file path.
         */
        void setSaveEnvironmentCallback(std::function<void(std::filesystem::path const&)> const& cb) { saveEnvironmentCallback = cb; }

        /**
         * @brief Registers callback triggered when selecting an asset in workspace hierarchy.
         * @param cb Callback function accepting selected asset ID.
         */
        void setSelectAssetCallback(std::function<void(ResourceId)> const& cb) { selectAssetCallback = cb; }

        /**
         * @brief Registers callback triggered when closing an asset tab or workspace entry.
         * @param cb Callback function accepting closed asset ID.
         */
        void setCloseAssetCallback(std::function<void(ResourceId)> const& cb) { closeAssetCallback = cb; }

        // Asset Management API

        /**
         * @brief Registers new asset into loaded workspace inventory.
         * @param asset Rvalue reference to LoadedAsset structure.
         */
        void addAsset(LoadedAsset&& asset);

        /**
         * @brief Removes asset from active workspace inventory.
         * @param id Asset identifier to remove.
         */
        void removeAsset(ResourceId id);

        /**
         * @brief Marks asset as modified or clean.
         * @param id Asset identifier.
         * @param dirty Dirty state flag.
         */
        void markAssetDirty(ResourceId id, bool dirty = true);

        /**
         * @brief Marks asset as saved and updates path/name metadata.
         * @param id Asset identifier.
         * @param newName Updated asset display name.
         * @param newPath Updated file system location.
         */
        void markAssetSaved(ResourceId id, std::string const& newName, std::filesystem::path const& newPath);

        // Resource Bin Access

        /**
         * @brief Accesses deferred resource bin manager.
         * @return Reference to ResourceBin instance.
         */
        ResourceBin& getResourceBin() { return m_resourceBin; }

        /**
         * @brief Creates ImGui Vulkan descriptor set from Vulkan image view.
         * @param imageView Vulkan ImageView handle.
         * @return VkDescriptorSet descriptor set handle.
         */
        static VkDescriptorSet createViewportDescriptorSet(vk::ImageView imageView);

        /**
         * @brief Creates unique RAII ImGui Vulkan descriptor set wrapper from image view.
         * @param imageView Vulkan ImageView handle.
         * @return UniqueViewportDescriptorSet instance.
         */
        static UniqueViewportDescriptorSet createViewportDescriptorSetUnique(vk::ImageView imageView);

        /**
         * @brief Checks if debug rendering mode is enabled.
         * @return True if debug output is active.
         */
        [[nodiscard]] bool isDebugModeEnabled() const { return debugModeEnabled; }

        /**
         * @brief Gets current main pass settings configuration.
         * @return MainPassSettings struct copy.
         */
        [[nodiscard]] engine::render::MainPassSettings getMainPassSettings() const { return settings; }

        /**
         * @brief Checks whether active viewport panel has valid dimensions.
         * @return True if viewport is visible and non-zero size.
         */
        [[nodiscard]] bool hasViewport() const;

        /**
         * @brief Checks if the viewport is currently focused.
         * @return True if the viewport is focused.
         */
        [[nodiscard]] bool isViewportFocused() const { return m_isViewportFocused; }

        /**
         * @brief Gets number of active output viewports according to current layout.
         * @return Output viewport panel count.
         */
        [[nodiscard]] uint32_t getVisibleOutputCount() const;

        /**
         * @brief Checks if viewport resources need to be recreated due to panel resize.
         * @return True if recreation is required.
         */
        [[nodiscard]] bool needViewPortResourcesRecreate() const;

        /**
         * @brief Gets camera mouse sensitivity parameter.
         * @return Camera sensitivity multiplier.
         */
        [[nodiscard]] float getCameraSensitivity() const { return camSensitivity; }

        /**
         * @brief Gets camera movement speed.
         * @return Movement speed units per second.
         */
        [[nodiscard]] float getCameraMoveSpeed() const { return camMoveSpeed; }

        /**
         * @brief Gets camera rotation speed parameter.
         * @return Rotation speed multiplier.
         */
        [[nodiscard]] float getCameraRotationSpeed() const { return camRotationSpeed; }

        /**
         * @brief Sets window/viewport resize mode state.
         * @param enabled True to enable resize mode.
         */
        void setResizeMode(bool enabled);

        /**
         * @brief Checks if editor is currently in window resize mode.
         * @return True if resize mode active.
         */
        [[nodiscard]] bool isResizeMode() const;

    private:
        /**
         * @brief Renders custom title bar and main menu strip.
         */
        void drawTitleBar();

        /**
         * @brief Renders viewport dockable panel containing rendering targets.
         */
        void drawViewportPanel();

        /**
         * @brief Renders main editor workspace, toolbars, and inspector panels.
         */
        void drawClientArea();

        /**
         * @brief Renders renderer settings inspector panel.
         */
        void drawRendererPanel();

        /**
         * @brief Renders camera navigation settings inspector panel.
         */
        void drawCameraPanel();

        /**
         * @brief Renders multi-viewport layout selector UI.
         */
        void drawViewportLayoutSelector();

        /**
         * @brief Renders debug buffer view selectors for multi-viewport split modes.
         */
        void drawDebugOutputSelectors();

        /**
         * @brief Renders individual viewport texture image inside ImGui context.
         * @param descriptorSet ImGui Vulkan texture descriptor.
         * @param size Target rendering dimension size.
         */
        static void drawViewportImage(VkDescriptorSet descriptorSet, ImVec2 size);

        /**
         * @brief Renders import dialog modal windows for scenes and environments.
         */
        void drawImportModals();

        /**
         * @brief Renders unsaved changes exit prompt modal window.
         */
        void drawExitConfirmationModal() const;

        /**
         * @brief Initiates application shutdown or presents exit prompt if dirty assets exist.
         */
        void tryExit();

        // Window & Render properties
        pal::WindowBase* window{nullptr};
        bool needRecreateViewPortResources{false};
        bool debugModeEnabled{false};
        bool m_openSceneModalRequested{false};
        bool m_openEnvModalRequested{false};
        bool m_resizeMode{false};
        bool m_isViewportFocused{false};

        ImVec2 g_actualMainWindowPos{0.0f, 0.0f};
        ImVec2 g_actualMainWindowSize{0.0f, 0.0f};

        ViewportLayoutMode viewportLayoutMode{ViewportLayoutMode::Single};
        engine::render::MainPassSettings settings{};
        vk::Extent2D viewportExtent{.width = 0, .height = 0};

        // Asset data
        std::vector<LoadedAsset> m_loadedAssets{};
        ResourceId m_selectedSceneId = 0;
        ResourceId m_selectedEnvironmentId = 0;

        ResourceBin m_resourceBin{};

        // Temporary data for import modals
        std::filesystem::path m_tempImportPath{};
        assets::scene_compiler::SceneCompilerOptions m_pendingSceneOptions{};
        engine::ibl::IblGenerationSettings m_pendingIblSettings{};
        std::vector<ResourceId> m_assetsPendingDeletion{};

        // Callbacks
        std::function<void(std::filesystem::path const&)> openSceneCallback{};
        std::function<void(std::filesystem::path const&)> openEnvironmentCallback{};
        std::function<void(std::filesystem::path const&, assets::scene_compiler::SceneCompilerOptions const&)> importSceneCallback{};
        std::function<void(std::filesystem::path const&, engine::ibl::IblGenerationSettings const&)> importEnvironmentCallback{};
        std::function<void(std::filesystem::path const&)> saveSceneCallback{};
        std::function<void(std::filesystem::path const&)> saveEnvironmentCallback{};

        std::function<void(ResourceId)> selectAssetCallback{};
        std::function<void(ResourceId)> closeAssetCallback{};

        // Helper variables
        VkDescriptorSet outDescriptorSet{VK_NULL_HANDLE};
        std::array<VkDescriptorSet, 4> viewportDebugDescriptorSets{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        ImFont* editorTitleFont{nullptr};
        float camMoveSpeed{5.0f};
        float camRotationSpeed{2.5f};
        float camSensitivity{0.1f};

        std::unique_ptr<pfd::open_file> sceneFileDialog{};
        std::unique_ptr<pfd::open_file> environmentFileDialog{};
        std::unique_ptr<pfd::open_file> importSceneFileDialog{};
        std::unique_ptr<pfd::open_file> importEnvFileDialog{};
        std::unique_ptr<pfd::save_file> saveFileDialog{};
        AssetType pendingSaveType{};
    };

} // namespace shuttle::editor::core

#endif // SHUTTLEENGINE_MAINWINDOW_HPP
