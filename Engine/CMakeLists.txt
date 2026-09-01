cmake_minimum_required(VERSION 3.20)

# Опция для принудительного использования SDL на Windows
option(SHUTTLE_FORCE_SDL "Force SDL2 backend on Windows" OFF)

# Список общих файлов PAL
set(PAL_SOURCES
        "src/PAL/Common/Window/WindowBase.cpp"
        # Добавь сюда остальные .cpp если они появятся
)

set(IMGUI_SOURCES
        "${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp"
        "${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
        "${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
        "${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
        "${PROJECT_SOURCE_DIR}/external/imgui/imgui_demo.cpp" # Для тестирования
        "${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp"
)

# --- Выбор Бэкенда ---
if(WIN32 AND NOT SHUTTLE_FORCE_SDL)
    set(IMGUI_SOURCES
            ${IMGUI_SOURCES}
            "${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_win32.cpp" # Бэкэнд для Win32
    )
    message(STATUS "PAL: Using Native Win32 Backend")
    list(APPEND PAL_SOURCES
            "src/PAL/Win32/Win32Platform.cpp"
    )
    # Системные либы Windows
    set(PLATFORM_LIBS user32 gdi32 shell32 Shcore Ole32)
    add_compile_definitions(SHUTTLE_PLATFORM_WIN32)
else()
    set(IMGUI_SOURCES
            ${IMGUI_SOURCES}
            "${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdl2.cpp"
            src/RenderGraph/RenderGraph.cpp
            src/RenderGraph/RenderGraph.hpp
    )
    message(STATUS "PAL: Using SDL2 Backend")
    list(APPEND PAL_SOURCES
            "src/PAL/Sdl/SdlPlatform.cpp"
    )
    set(PLATFORM_LIBS ${SDL2Libraries})
    add_compile_definitions(SHUTTLE_PLATFORM_SDL)
    if(SHUTTLE_FORCE_SDL)
        add_compile_definitions(SHUTTLE_FORCE_SDL)
    endif()
endif()

add_executable(
    Engine
    ${IMGUI_SOURCES}
    ${PAL_SOURCES}
    src/PAL/Common/Events/Events.hpp
    src/PAL/Common/Keyboard/KeyState.hpp
    src/PAL/Common/Window/WindowBase.hpp
    src/PAL/Common/Window/MainWindow.hpp
    src/PAL/Common/Window/ModalDialogWindow.hpp
    src/PAL/Common/Window/DetachedPanelWindow.hpp
    src/PAL/Common/Window/WindowBase.cpp
    src/PAL/Platform.hpp
    src/PAL/Common/Window/Decorations/HitTestTypes.hpp
    src/PAL/Common/Window/Decorations/ResizeLayout.hpp
    src/PAL/Common/Window/Decorations/TitlebarLayout.hpp
    src/PAL/Common/Window/Decorations/WindowDecorations.hpp
    src/PAL/Common/Window/Decorations/WindowDecorationFlags.hpp
    src/PAL/Common/Display/DisplayTypes.hpp
    src/PAL/Common/Display/MonitorInfo.hpp
    src/PAL/Common/Display/MonitorHandle.hpp
    src/PAL/Win32/Win32Platform.cpp
    src/PAL/Win32/Win32Platform.hpp
    src/PAL/Common/Window/WindowType.hpp
    src/PAL/Common/Window/ShowMode.hpp
    src/Application/Application.cpp
    src/Application/Application.hpp
    src/PAL/Sdl/SdlPlatform.cpp
    src/VulkanDebugger/VulkanDebugger.cpp
    src/ImageLoader/Image.hpp
    src/ImageLoader/Image.cpp
    src/VulkanHelperFunctions/VulkanHelperFunctions.cpp
    src/Camera/Camera.cpp
    src/DeviceAllocator/DeviceAllocator.cpp
    src/CameraController/CameraController.cpp
    src/CameraController/CameraController.hpp
    src/main.cpp
    src/Render/Render.hpp
    src/Render/SceneResources.cpp
    src/Render/EnvironmentResources.cpp
    src/UiRender/UiRender.cpp
    src/UiRender/UiRender.hpp
    src/FrameManager/FrameManager.cpp
    src/FrameManager/FrameManager.hpp
    src/SwapchainFactory/SwapchainFactory.cpp
    src/SwapchainFactory/SwapchainFactory.hpp
    src/RetireController/RetireController.cpp
    src/RetireController/RetireController.hpp
    src/MainWindow/MainWindow.cpp
    src/MainWindow/MainWindow.hpp
    src/Render/MainPass.cpp
    src/Render/MainPass.hpp
    src/Render/Common.hpp
    src/Render/uiPass.cpp
    src/Render/uiPass.hpp
    src/Render/CameraSystem.cpp
    src/Render/CameraSystem.hpp
    src/Render/WorldTransformUpdatePass.cpp
    src/Render/WorldTransformUpdatePass.hpp
    src/Render/PrefixSumPass.cpp
    src/Render/PrefixSumPass.hpp
    src/Render/InstanceRemapPass.cpp
    src/Render/InstanceRemapPass.hpp
    src/DeviceAllocator/VirtualBlock.cpp
    src/DeviceAllocator/VirtualBlock.hpp
    src/Render/DescriptorHeapSet.cpp
    src/Render/DescriptorHeapSet.hpp
    src/Render/FallbackTextures.hpp
    src/Render/FallbackTextures.cpp
    src/Render/CommonResources.cpp
    src/Render/CommonResources.hpp
    src/SceneDataLoader/SceneDataLoader.cpp
    src/SceneDataLoader/SceneDataLoader.hpp
    src/Render/SceneData.hpp
    src/Render/MeshInstancesCountPass.cpp
    src/Render/MeshInstancesCountPass.hpp
)

target_include_directories(
    Engine
    PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}/SDL2
        ${PROJECT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/external/cmft/include
        ${PROJECT_SOURCE_DIR}/external/VulkanMemoryAllocator/include
        ${PROJECT_SOURCE_DIR}/external/stb
        ${PROJECT_SOURCE_DIR}/external/vk-bootstrap/src
        ${PROJECT_SOURCE_DIR}/external/tinyddsloader
        ${PROJECT_SOURCE_DIR}/external/mio/single_include
        ${PROJECT_SOURCE_DIR}/Assets/SceneCompiler/include
        ${PROJECT_SOURCE_DIR}/Assets/EnvironmentCompiler/include
        ${PROJECT_SOURCE_DIR}/Assets/TextureCompiler/include
        ${PROJECT_SOURCE_DIR}/include)

target_compile_definitions(
    Engine
    PRIVATE
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_IMPL_VULKAN_HAS_EXTENSION
        IMGUI_IMPL_VULKAN_USE_RUNTIME_BINDINGS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Тоже полезно
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS)

target_link_libraries(Engine PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(Engine PRIVATE ShuttleAssetFormats)
target_link_libraries(Engine PRIVATE ShuttleSceneCompiler)
target_link_libraries(Engine PRIVATE ShuttleEnvironmentCompiler)
# Найди строку target_link_libraries для Engine и добавь туда comctl32:
if(WIN32)
    target_link_libraries(Engine PRIVATE
            dwmapi      # Для ImGui Win32 (DwmIsCompositionEnabled, DwmEnableBlurBehindWindow)
            shcore      # Для High DPI (GetDpiForMonitor)
            comctl32
    )
endif()


add_dependencies(Engine CompileShaders)
add_dependencies(Engine CopyResources)
