# Target for copying assets to the build directory
add_custom_target(
        CopyResources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/engine"
        "${CMAKE_CURRENT_BINARY_DIR}/engine"
)