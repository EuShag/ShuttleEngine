cmake_minimum_required(VERSION 3.20)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp            # Для тестирования
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp   # Бэкэнд для SDL2
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
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
        "src/Terrain/Terrain.hpp"
        "src/PbrRender/Render.cpp"
        "src/PbrRender/Render.hpp"
        "src/AssimpLoader/AssimpLoader.cpp"
        "src/AssimpLoader/AssimpLoader.hpp"
        "src/HostRenderData/HostRenderData.cpp"
        "src/DeviceAllocator/DeviceAllocator.cpp"
        "src/HostImageProcessor/HostImageProcessor.cpp"
        "src/HostImageProcessor/HostImageProcessor.hpp"
        "src/HostImageProcessor/HostImageProcessor.hpp"
        "src/PbrRender/RenderInit.cpp"
        "src/PbrRender/PrepareMaterial.cpp"
        "src/CameraController/CameraController.cpp"
        "src/CameraController/CameraController.hpp"
        "src/main.cpp"
        "src/PbrRender/PrepareMesh.cpp"
        "src/UiRender/UiRender.cpp"
        "src/UiRender/UiRender.hpp"
        "src/ApplicationCotroller/ApplicationController.cpp"
        "src/ApplicationCotroller/ApplicationController.hpp"
        "src/Painters/SunLightControlPanel/SunLightControlPanel.cpp"
        "src/Painters/SunLightControlPanel/SunLightControlPanel.hpp"
        "src/FrameManager/FrameManager.cpp"
        "src/FrameManager/FrameManager.hpp"
        "src/SwapchainFactory/SwapchainFactory.cpp"
        "src/SwapchainFactory/SwapchainFactory.hpp"
        "src/RetireController/RetireController.cpp"
        "src/RetireController/RetireController.hpp"
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
target_link_libraries(Engine PRIVATE assimp::assimp)
target_link_libraries(Engine PRIVATE mimalloc)
target_include_directories(Engine PRIVATE "${imgui_SOURCE_DIR}")
target_include_directories(Engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(Engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/VMA/include")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/stb")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/assimp/include")
target_include_directories(Engine SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/_deps/assimp-build/include")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/concurrentqueue")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/mimalloc/include")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/taskflow")
target_include_directories(Engine SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/vk-bootstrap/src")

add_dependencies(Engine CopyAssets)
add_dependencies(Engine CompileShaders)
add_dependencies(Engine CopyCubeMapImages)
add_dependencies(Engine assimp)