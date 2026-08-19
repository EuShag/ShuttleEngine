# Target for copying assets to the build directory
add_custom_target(
    CopyResources
    ALL
    COMMAND
        ${CMAKE_COMMAND}
        -E
        copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/engine"
        "${CMAKE_CURRENT_BINARY_DIR}/engine"
COMMAND

${CMAKE_COMMAND}
        -E
        copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "${CMAKE_CURRENT_BINARY_DIR}/fonts"
    COMMENT "Copying resources to build directory")
