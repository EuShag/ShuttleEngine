cmake_minimum_required(VERSION 3.20)

set(IMGUI_SOURCES
    "${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp"
    "${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
    "${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
    "${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
    "${PROJECT_SOURCE_DIR}/external/imgui/imgui_demo.cpp" # Для тестирования
    "${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp"
)

add_executable(
    engine
    ${IMGUI_SOURCES}
    vulkan_context/vulkan_context.cpp
    vulkan_context/vulkan_context.hpp
    src/command_buffer_provider/command_buffer_provider.cpp
    src/command_buffer_provider/command_buffer_provider.hpp
    src/command_buffer_provider/command_pool.cpp
    src/command_buffer_provider/command_pool.hpp
    src/command_buffer_provider/command_buffer_token.cpp
    src/command_buffer_provider/command_buffer_token.hpp
    src/command_buffer_provider/command_buffer_recorder.cpp
    src/command_buffer_provider/command_buffer_recorder.hpp
    src/command_buffer_provider/dedicated_command_pool.cpp
    src/command_buffer_provider/dedicated_command_pool.hpp
    src/fence_waiter/fence_waiter.cpp
    src/fence_waiter/fence_waiter.hpp
    src/application/application.cpp
    src/application/application.hpp
    src/vulkan_debugger/vulkan_debugger.cpp
    src/image_loader/image.hpp
    src/image_loader/image.cpp
    src/vulkan_helper_functions/vulkan_helper_functions.cpp
    src/camera/camera.cpp
    src/device_allocator/device_allocator.cpp
    src/camera_controller/camera_controller.cpp
    src/camera_controller/camera_controller.hpp
    src/main.cpp
    src/render/render.hpp
    src/main_window/main_window.cpp
    src/main_window/main_window.hpp
    src/render/main_pass.cpp
    src/render/main_pass.hpp
    src/render/common.hpp
    src/render/camera_system.cpp
    src/render/camera_system.hpp
    src/device_allocator/virtual_block.cpp
    src/device_allocator/virtual_block.hpp
    src/render/descriptor_heap_set.cpp
    src/render/descriptor_heap_set.hpp
    src/render/fallback_textures.hpp
    src/render/common_resources.hpp
    src/scene_data_loader/scene_data_loader.cpp
    src/scene_data_loader/scene_data_loader.hpp
    src/render/scene_data.hpp
    src/im_ui_context/im_ui_context.hpp
    src/im_ui_context/im_ui_context.cpp
    src/engine/im_ui_context.cpp
    src/engine/engine_impl.cpp
    src/engine/engine_impl.hpp
    src/engine/resource.hpp
    src/engine/handle.hpp
    src/engine/pipeline.cpp
    src/engine/viewport_resources.hpp
    src/engine/scenes.cpp
    src/environment_data_loader/environment_data_loader.cpp
    src/environment_data_loader/environment_data_loader.hpp
    src/engine/common_resources.cpp
    src/engine/environments.cpp
    src/engine/swapchain.cpp
    src/engine/render_scene_ui.cpp
    src/engine/render_ui_only.cpp
    src/engine/create.cpp
    src/engine/i_engine_impl.hpp
    include/result_code.hpp
)

target_include_directories(
    engine
    PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/external/cmft/include
        ${PROJECT_SOURCE_DIR}/external/concurrentqueue
        ${PROJECT_SOURCE_DIR}/external/VulkanMemoryAllocator/include
        ${PROJECT_SOURCE_DIR}/external/stb
        ${PROJECT_SOURCE_DIR}/external/vk-bootstrap/src
        ${PROJECT_SOURCE_DIR}/external/tinyddsloader
        ${PROJECT_SOURCE_DIR}/external/mio/single_include
        ${PROJECT_SOURCE_DIR}/assets/scene_compiler/include
        ${PROJECT_SOURCE_DIR}/assets/environment_compiler/include
        ${PROJECT_SOURCE_DIR}/assets/texture_compiler/include
        ${PROJECT_SOURCE_DIR}/external/mimalloc/include
        ${PROJECT_SOURCE_DIR}/include)

target_compile_definitions(
    engine
    PRIVATE
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_IMPL_VULKAN_HAS_EXTENSION
        IMGUI_IMPL_VULKAN_USE_RUNTIME_BINDINGS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Тоже полезно
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS)

target_link_libraries(engine PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(engine PRIVATE async::async)
target_link_libraries(engine PRIVATE pal::pal)
target_link_libraries(engine PRIVATE shuttle_asset_formats)
target_link_libraries(engine PRIVATE shuttle_scene_compiler)
target_link_libraries(engine PRIVATE shuttle_environment_compiler)
add_dependencies(engine compile_shaders)
add_dependencies(engine copy_resources)
