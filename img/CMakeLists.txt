# target for copying the cube map images to the output directory (back.png, front.png, left.png, right.png, up.png, down.png)
# copy back.png to back.png in the output directory
add_custom_target(
        CopyCubeMapImages ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/back.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/front.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/left.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/right.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/up.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/down.png"
        ${CMAKE_CURRENT_BINARY_DIR}
)