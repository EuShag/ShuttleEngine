# ============================================================
# Shader compilation
# ============================================================

find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(
    GLOB SHADER_SOURCES
    CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.comp"
    "${SHADER_SOURCE_DIR}/*.geom"
    "${SHADER_SOURCE_DIR}/*.tesc"
    "${SHADER_SOURCE_DIR}/*.tese")

file(
    GLOB
    SHADER_INCLUDES
    CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.glsl")

set(SHADER_SPV_FILES)

foreach(SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)

    set(SHADER_SPV "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT "${SHADER_SPV}"
        COMMAND
            ${GLSLC_EXECUTABLE}
            --target-env=vulkan1.2
            -I "${SHADER_SOURCE_DIR}"
            "${SHADER_SOURCE}"
            -o "${SHADER_SPV}"
        DEPENDS "${SHADER_SOURCE}" ${SHADER_INCLUDES}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM)

    list(APPEND SHADER_SPV_FILES "${SHADER_SPV}")
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SHADER_SPV_FILES})
