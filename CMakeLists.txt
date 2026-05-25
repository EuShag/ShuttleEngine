cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
		Assimp
		GIT_REPOSITORY https://github.com/assimp/assimp
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/assimp
		CMAKE_ARGS
			-DASSIMP_NO_WERROR=ON
			-DASSIMP_BUILD_ZLIB=ON
)

FetchContent_MakeAvailable(Assimp)

FetchContent_Declare(
		ConcurrentQueue
		GIT_REPOSITORY https://github.com/cameron314/concurrentqueue
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/concurrentqueue
)

FetchContent_MakeAvailable(ConcurrentQueue)

FetchContent_declare(
		Mimalloc
		GIT_REPOSITORY https://github.com/microsoft/mimalloc main
		GIT_TAG main
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/mimalloc
)

FetchContent_MakeAvailable(Mimalloc)

#TaskFlow
FetchContent_Declare(
		TaskFlow
		GIT_REPOSITORY https://github.com/taskflow/taskflow
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/taskflow
)

FetchContent_MakeAvailable(TaskFlow)

project(HelloTriangle)

add_executable(HelloTriangle
	"HelloTriangle.cpp"
	"src/Sdl/SdlLibrary/SdlLibrary.cpp" 
	"src/Sdl/SdlWindow/SdlWindow.cpp" 
	"src/VulkanInstanceBuilder/VulkanInstanceBuilder.cpp"
	"src/VulkanDebugger/VulkanDebugger.cpp" 
	"src/VulkanStructureChain/VulkanStructureChain.cpp"
	"src/ImageLoader/Image.cpp"  
	"src/VulkanDeviceAllocator/VulkanDeviceAllocator.cpp" 
	"src/VulkanHelperFunctions/VulkanHelperFunctions.cpp"
	"src/VulkanCopyManager/VulkanCopyManager.cpp"
	"src/Camera/Camera.cpp" "src/Skybox/Skybox.cpp"
		src/Texture/Texture.hpp
		src/Texture/Texture.cpp
		src/PbrModelLoader/Scene.hpp
)

find_package(Vulkan REQUIRED)
message("Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")
set(SDL2Libraries
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib)

target_include_directories(HelloTriangle PRIVATE ${Vulkan_INCLUDE_DIRS})

target_link_libraries(HelloTriangle PRIVATE ${SDL2Libraries})
target_link_libraries(HelloTriangle PRIVATE assimp::assimp)
target_link_libraries(HelloTriangle PRIVATE mimalloc)
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/VMA/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/_deps/assimp-build/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/concurrentqueue")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/mimalloc/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/taskflow")

add_subdirectory(shaders)
add_subdirectory(img)
add_subdirectory(assets)
add_dependencies(HelloTriangle CopyAssets)
add_dependencies(HelloTriangle CompileShaders)
add_dependencies(HelloTriangle CopyCubeMapImages)
add_dependencies(HelloTriangle assimp)
