add_library(ShuttleTextureCompiler STATIC)

target_sources(
    ShuttleTextureCompiler
    PRIVATE
        src/TextureCompiler.cpp
        ${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/rgbcx.cpp
        ${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/rgbcx.h
        src/ispc_texcomp/ispc_texcomp.cpp
        src/ispc_texcomp/gcc/kernel.o)

target_include_directories(
    ShuttleTextureCompiler
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external/stb
        ${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/
        ${PROJECT_SOURCE_DIR}/external/tinyddsloader
        ${PROJECT_SOURCE_DIR}/external/ispc_texcomp
        ${PROJECT_SOURCE_DIR}/external/mio/single_include)

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    target_compile_options(ShuttleTextureCompiler PUBLIC ${OpenMP_CXX_FLAGS})

    target_link_options(ShuttleTextureCompiler PUBLIC ${OpenMP_CXX_FLAGS})

    target_link_libraries(ShuttleTextureCompiler PUBLIC OpenMP::OpenMP_CXX)
endif()

target_link_libraries(
    ShuttleTextureCompiler
    PUBLIC
        ShuttleAssetFormats
        ShuttleAssetCore
        ShuttleSceneCompiler
        ShuttleEnvironmentCompiler
        OpenMP::OpenMP_CXX
    PRIVATE)

target_compile_features(ShuttleTextureCompiler PUBLIC cxx_std_20)

add_dependencies(ShuttleTextureCompiler)
