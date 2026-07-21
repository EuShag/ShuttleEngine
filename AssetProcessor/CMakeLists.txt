
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # CMAKE_SOURCE_DIR всегда указывает на корень ShuttleEngine, где лежат ваши папки
    set(PRECOMPILED_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/src/ispc_texcomp/msvc/kernel.obj")
    add_compile_options(/arch:AVX2 /O2)
    message(STATUS "[AssetProcessor] Тулчейн MSVC -> Линкуем .obj ядро")
else()
    # Сюда заходит ваш MinGW GCC
    set(PRECOMPILED_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/src/ispc_texcomp/gcc/kernel.o")
    add_compile_options(-mavx2 -mfma -O3)
    message(STATUS "[AssetProcessor] Тулчейн MinGW/GCC -> Линкуем .o ядро")
endif()

add_executable(AssetProcessor
    include/BlobLayout.hpp
    include/EnvironmentFormat.hpp
    src/main.cpp
    src/TextureImporter/TextureImporter.cpp
    src/TextureImporter/TextureImporter.hpp
    src/SceneImporter/SceneImporter.cpp
    src/SceneImporter/SceneImporter.hpp
    src/TextureImporter/TextureImporter_init.cpp
    src/ispc_texcomp/ispc_texcomp.cpp
    src/ispc_texcomp/ispc_texcomp.h
    src/ispc_texcomp/kernel_ispc.h
    src/EnvironmentBlobWriter/EnvironmentBlobWriter.cpp
    src/EnvironmentBlobWriter/EnvironmentBlobWriter.hpp
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/rgbcx.cpp"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/rgbcx.h"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/bc7enc.h"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/bc7enc.cpp"
    ${PRECOMPILED_KERNEL}
)

if(NOT MSVC)
    target_link_options(AssetProcessor PRIVATE "-Wl,--defsym,__chkstk=___chkstk_ms")
endif()

target_include_directories(AssetProcessor PRIVATE include)

target_link_libraries(AssetProcessor PRIVATE
    assimp
    OpenMP::OpenMP_CXX
    meshoptimizer
)

add_executable(cmft_test
        tests/CmftTest.cpp
        src/EnvironmentBlobWriter/EnvironmentBlobWriter.cpp
        src/EnvironmentBaker/EnvironmentBaker.cpp
)

target_link_libraries(
        cmft_test
        PRIVATE
        cmft
)

target_include_directories(
        cmft_test
        PRIVATE
        include
        ${Vulkan_INCLUDE_DIRS}
)

target_compile_definitions(AssetProcessor PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if(OpenMP_CXX_FOUND)
    # Замените AssetProcessor на имя вашей цели, если оно отличается
    target_compile_options(AssetProcessor PRIVATE -fopenmp)
    target_link_libraries(AssetProcessor PRIVATE -lgomp)
endif()

target_include_directories(AssetProcessor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(AssetProcessor PRIVATE
    "${PROJECT_SOURCE_DIR}/external/tinyddsloader"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo"
    "${PROJECT_SOURCE_DIR}/external/stb"
    "${PROJECT_SOURCE_DIR}/external/assimp/include"
    "${PROJECT_BINARY_DIR}/external/assimp/include"
    "${PROJECT_SOURCE_DIR}/external/meshoptimizer"
    "${PROJECT_SOURCE_DIR}/external/cmft/include"
    ${Vulkan_INCLUDE_DIRS}
)

add_dependencies( AssetProcessor
    assimp
)