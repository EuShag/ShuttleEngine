add_library(ShuttleAssetFormats INTERFACE)

target_include_directories(
        ShuttleAssetFormats
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
        ShuttleAssetFormats
        INTERFACE
        ShuttleAssetCore
)