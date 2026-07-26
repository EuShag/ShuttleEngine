add_library(ShuttleSceneCompiler STATIC)

target_sources(ShuttleSceneCompiler
        PRIVATE

        # facade
        src/SceneCompiler.cpp

        # importers
        src/Importers/Assimp/AssimpSceneImporter.cpp

        # texture
        src/Texture/SceneTextureResolver.cpp
        src/Texture/SceneTextureCompiler.cpp

        # builders
        src/Builders/GeometryBuilder.cpp
        src/Builders/MaterialBuilder.cpp
        src/Builders/AnimationBuilder.cpp
        src/Builders/SceneGraphBuilder.cpp
        src/Builders/SceneBuilder.cpp
        src/Builders/LightingBuilder.cpp

        # serialization
        src/Serialization/CompiledSceneBlobWriter.cpp
)

target_include_directories(ShuttleSceneCompiler
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../Formats/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../TextureCompiler/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Core/include
        ${PROJECT_SOURCE_DIR}/external/stb
        ${PROJECT_SOURCE_DIR}/external/mio/single_include
)

target_link_libraries(ShuttleSceneCompiler
        PUBLIC
        ShuttleAssetFormats
        ShuttleTextureCompiler
        PRIVATE
        assimp
        meshoptimizer
)

target_compile_features(ShuttleSceneCompiler
        PUBLIC
        cxx_std_23
)