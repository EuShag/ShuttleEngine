add_executable(AssetToolkit)

target_sources(AssetToolkit
        PRIVATE
        src/Main.cpp
        src/AssetProcessorApp/AssetProcessorApp.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdlrenderer2.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_demo.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
        ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdl2.cpp

)

target_include_directories(AssetToolkit
        PRIVATE
        ${PROJECT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIRS}
        ../Core/include
        ../EnvironmentCompiler/include
        ../EnvironmentCompiler/src
        ../SceneCompiler/include
        ../SceneCompiler/src
        ../Formats/include
        ../TextureCompiler/include
)

target_link_libraries(AssetToolkit
        PRIVATE

        ShuttleSceneCompiler
        ShuttleTextureCompiler

        ShuttleAssetCore
        ShuttleAssetFormats
        ShuttleEnvironmentCompiler

        ${SDL2Libraries}

        OpenMP::OpenMP_CXX
)

target_compile_features(AssetToolkit
        PRIVATE
        cxx_std_23
)

add_dependencies(AssetToolkit
        ShuttleSceneCompiler
        ShuttleTextureCompiler
        ShuttleEnvironmentCompiler
        ShuttleAssetCore
        ShuttleAssetFormats
)