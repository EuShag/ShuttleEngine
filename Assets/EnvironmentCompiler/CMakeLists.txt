cmake_minimum_required(VERSION 3.28)

add_library(ShuttleEnvironmentCompiler STATIC
        include/Assets/EnvironmentCompiler/CpuIblGenerator.hpp)

target_sources(
    ShuttleEnvironmentCompiler
    PRIVATE
        src/EnvironmentCompiler.cpp
        src/EnvironmentBaker.cpp
        src/CompiledEnvironmentBlobWriter.cpp)

target_include_directories(
    ShuttleEnvironmentCompiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../Formats/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../TextureCompiler/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Core/include
        ${Vulkan_INCLUDE_DIRS})

target_link_libraries(
    ShuttleEnvironmentCompiler
    PUBLIC
        ShuttleAssetFormats
        ShuttleTextureCompiler
        ShuttleAssetCore)

target_compile_features(ShuttleEnvironmentCompiler PUBLIC cxx_std_23)
