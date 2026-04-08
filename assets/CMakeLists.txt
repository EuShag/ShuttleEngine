# Target for copying assets to the build directory
add_custom_target(
        CopyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/models"
        "${CMAKE_CURRENT_BINARY_DIR}/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/skybox"
        "${CMAKE_CURRENT_BINARY_DIR}/skybox"
)