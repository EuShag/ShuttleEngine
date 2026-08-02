cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(ShuttleEngine)

find_package(Vulkan REQUIRED)
find_package(OpenMP REQUIRED)
message("Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")
set(SDL2Libraries
    "${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib"
    "${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib")

# Путь, где соберутся все исполняемые файлы
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
# Чтобы DLL уходили в lib, CMake требует выставлять LIBRARY_OUTPUT_DIRECTORY, но Windows ищет DLL только рядом с EXE или
# в PATH.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Настройки стабильного Assimp (Включаем фабричный zlib)
set(ASSIMP_BUILD_ZLIB
    ON
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL
    OFF
    CACHE BOOL "" FORCE)

set(OPTION_ENABLE_ALL_APPS
    OFF
    CACHE BOOL "Build Compressonator Applications" FORCE)
set(OPTION_BUILD_PLUGINS
    OFF
    CACHE BOOL "Build Compressonator Plugins" FORCE)
set(OPTION_BUILD_TESTS
    OFF
    CACHE BOOL "Build Compressonator Tests" FORCE)

if(MSVC)
    add_compile_options(/FS)
endif()

include(
    cmake/cmft_target.cmake
)
add_subdirectory(
    Assets
)
add_subdirectory(
    external/meshoptimizer
)
add_subdirectory(
    external/vk-bootstrap
)
add_subdirectory(
    shaders
)
add_subdirectory(
    Engine
)
add_subdirectory(
    resources
)

set(ASSIMP_BUILD_TESTS
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL
    OFF
    CACHE BOOL "" FORCE)

set(ASSIMP_NO_EXPORT
    ON
    CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT
    OFF
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT
    OFF
    CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_FBX_IMPORTER
    ON
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER
    ON
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER
    ON
    CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER
    ON
    CACHE BOOL "" FORCE)

add_subdirectory(
    external/assimp
)

if(MSVC AND TARGET assimp)
    target_compile_options(assimp PRIVATE /FS)
endif()
