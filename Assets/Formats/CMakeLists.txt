add_library(shuttle_asset_formats INTERFACE)

target_include_directories(
    shuttle_asset_formats
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS})

target_link_libraries(shuttle_asset_formats INTERFACE shuttle_asset_core)
