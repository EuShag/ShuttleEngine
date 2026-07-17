
add_executable(AssetProcessor
    include/BlobLayout.hpp
    src/main.cpp
    src/TextureImporter/TextureImporter.cpp
    src/TextureImporter/TextureImporter.hpp
    src/SceneImporter/SceneImporter.cpp
    src/SceneImporter/SceneImporter.hpp
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/rgbcx.cpp"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/rgbcx.h"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/bc7enc.h"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo/bc7enc.cpp"
)

target_include_directories(AssetProcessor SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})

target_link_libraries(AssetProcessor PRIVATE
    ktx
    OpenMP::OpenMP_CXX
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
    "${PROJECT_SOURCE_DIR}/external/cgltf"
    "${PROJECT_SOURCE_DIR}/external/KTX-Software/lib/include"
    "${PROJECT_SOURCE_DIR}/external/KTX-Software/external/dfdutils"
    "${PROJECT_SOURCE_DIR}/external/bc7enc_rdo"
    "${PROJECT_SOURCE_DIR}/external/stb"
    "${PROJECT_SOURCE_DIR}/external/assimp/include"
    "${PROJECT_BINARY_DIR}/external/assimp"
    "${ASSIMP_GENERATED_DIR}/include"
    ${Vulkan_INCLUDE_DIRS}
)

add_dependencies( AssetProcessor
    assimp
    ktx
)