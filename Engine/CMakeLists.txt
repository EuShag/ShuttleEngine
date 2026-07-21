cmake_minimum_required(VERSION 3.20)

set(IMGUI_SOURCES
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_demo.cpp            # Для тестирования
        ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdl2.cpp   # Бэкэнд для SDL2
        ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp
)

add_executable(Engine
        ${IMGUI_SOURCES}
        "src/Sdl/SdlLibrary/SdlLibrary.cpp"
        "src/Sdl/SdlWindow/SdlWindow.cpp"
        "src/VulkanDebugger/VulkanDebugger.cpp"
        "src/ImageLoader/Image.hpp"
        "src/ImageLoader/Image.cpp"
        "src/VulkanHelperFunctions/VulkanHelperFunctions.cpp"
        "src/Camera/Camera.cpp"
        "src/Skybox/Skybox.cpp"
        "src/Terrain/Terrain.cpp"
        "src/Terrain/Terrain.hpp"
        "src/PbrRender/Render.cpp"
        "src/PbrRender/Render.hpp"
        "src/BlobLoader/BlobLoader.hpp"
        "src/BlobLoader/BlobLoader.cpp"
        "src/HostRenderData/HostRenderData.cpp"
        "src/DeviceAllocator/DeviceAllocator.cpp"
        "src/PbrRender/RenderInit.cpp"
        "src/PbrRender/PrepareMaterial.cpp"
        "src/CameraController/CameraController.cpp"
        "src/CameraController/CameraController.hpp"
        "src/main.cpp"
        "src/PbrRender/PrepareMesh.cpp"
        "src/UiRender/UiRender.cpp"
        "src/UiRender/UiRender.hpp"
        "src/Painters/SunLightControlPanel/SunLightControlPanel.cpp"
        "src/Painters/SunLightControlPanel/SunLightControlPanel.hpp"
        "src/FrameManager/FrameManager.cpp"
        "src/FrameManager/FrameManager.hpp"
        "src/SwapchainFactory/SwapchainFactory.cpp"
        "src/SwapchainFactory/SwapchainFactory.hpp"
        "src/RetireController/RetireController.cpp"
        "src/RetireController/RetireController.hpp"
        "src/EnvironmentBlobLoader/EnvironmentBlobLoader.cpp"
        "src/EnvironmentBlobLoader/EnvironmentBlobLoader.hpp"
)

target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})

target_compile_definitions(Engine
        PRIVATE
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_IMPL_VULKAN_HAS_EXTENSION
        IMGUI_IMPL_VULKAN_USE_RUNTIME_BINDINGS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Тоже полезно

        GLM_FORCE_DEPTH_ZERO_TO_ONE
        VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)

target_link_libraries(Engine PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(Engine PRIVATE ${SDL2Libraries})
target_include_directories(Engine PRIVATE "${Vulkan_INCLUDE_DIRS}/SDL2"
                                            "${PROJECT_SOURCE_DIR}/external/imgui"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/include"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/src"
                                            "${PROJECT_SOURCE_DIR}/external/cmft/include"
                                            "${PROJECT_SOURCE_DIR}/AssetProcessor/include"
                                            "${PROJECT_SOURCE_DIR}/external/VulkanMemoryAllocator/include"
                                            "${PROJECT_SOURCE_DIR}/external/stb"
                                            "${PROJECT_SOURCE_DIR}/external/vk-bootstrap/src"
                                            "${PROJECT_SOURCE_DIR}/external/tinyddsloader")

add_dependencies(Engine CopyAssets)
add_dependencies(Engine CompileShaders)
add_dependencies(Engine CopyCubeMapImages)