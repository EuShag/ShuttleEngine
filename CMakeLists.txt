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

FetchContent_Declare(
		imgui
		GIT_REPOSITORY https://github.com/ocornut/imgui
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)

FetchContent_MakeAvailable(imgui)

message(STATUS "ImGui source dir: ${imgui_SOURCE_DIR}" )

set(IMGUI_SOURCES
		${imgui_SOURCE_DIR}/imgui.cpp
		${imgui_SOURCE_DIR}/imgui_draw.cpp
		${imgui_SOURCE_DIR}/imgui_widgets.cpp
		${imgui_SOURCE_DIR}/imgui_tables.cpp
		${imgui_SOURCE_DIR}/imgui_demo.cpp            # Для тестирования
		${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp   # Бэкэнд для SDL2
		${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
		src/Painters/SunLightControlPanel/SunLightControlPanel.cpp
		src/Painters/SunLightControlPanel/SunLightControlPanel.hpp
		src/FrameManager/FrameManager.cpp
		src/FrameManager/FrameManager.hpp
		src/SwapchainFactory/SwapchainFactory.cpp
		src/SwapchainFactory/SwapchainFactory.hpp
		src/RetireController/RetireController.cpp
		src/RetireController/RetireController.hpp
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

#vk-bootstrap
FetchContent_Declare(
		VkBootstrap
		GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
		GIT_TAG main
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vk-bootstrap
)

FetchContent_MakeAvailable(VkBootstrap)

project(HelloTriangle)

add_executable(HelloTriangle
	"src/Sdl/SdlLibrary/SdlLibrary.cpp" 
	"src/Sdl/SdlWindow/SdlWindow.cpp"
	"src/VulkanDebugger/VulkanDebugger.cpp"
	"src/ImageLoader/Image.hpp"
	"src/ImageLoader/Image.cpp"
	"src/VulkanHelperFunctions/VulkanHelperFunctions.cpp"
	"src/Camera/Camera.cpp"
	"src/Skybox/Skybox.cpp"
	src/Terrain/Terrain.cpp
	src/Terrain/Terrain.hpp
	src/Terrain/Terrain.hpp
	src/PbrRender/Render.cpp
	src/PbrRender/Render.hpp
	src/AssimpLoader/AssimpLoader.cpp
	src/AssimpLoader/AssimpLoader.hpp
	src/HostRenderData/HostRenderData.cpp
	src/DeviceAllocator/DeviceAllocator.cpp
	src/HostImageProcessor/HostImageProcessor.cpp
	src/HostImageProcessor/HostImageProcessor.hpp
	src/HostImageProcessor/HostImageProcessor.hpp
	src/PbrRender/RenderInit.cpp
	src/PbrRender/PrepareMaterial.cpp
	src/CameraController/CameraController.cpp
	src/CameraController/CameraController.hpp
	main.cpp
	src/PbrRender/PrepareMesh.cpp
        src/UiRender/UiRender.cpp
        src/UiRender/UiRender.hpp
		${IMGUI_SOURCES}
		src/ApplicationCotroller/ApplicationController.cpp
		src/ApplicationCotroller/ApplicationController.hpp
)

find_package(Vulkan REQUIRED)
message("Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")
set(SDL2Libraries
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib)

target_include_directories(HelloTriangle PRIVATE ${Vulkan_INCLUDE_DIRS})

target_compile_definitions(HelloTriangle
	PRIVATE
		IMGUI_IMPL_VULKAN_NO_PROTOTYPES
		IMGUI_IMPL_VULKAN_HAS_EXTENSION
		IMGUI_IMPL_VULKAN_USE_RUNTIME_BINDINGS
		IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Тоже полезно

		GLM_FORCE_DEPTH_ZERO_TO_ONE
		VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)

target_link_libraries(HelloTriangle PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(HelloTriangle PRIVATE ${SDL2Libraries})
target_link_libraries(HelloTriangle PRIVATE assimp::assimp)
target_link_libraries(HelloTriangle PRIVATE mimalloc)
target_include_directories(HelloTriangle PRIVATE "${imgui_SOURCE_DIR}")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/VMA/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/_deps/assimp-build/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/concurrentqueue")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/mimalloc/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/taskflow")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/vk-bootstrap/src")

add_subdirectory(shaders)
add_subdirectory(img)
add_subdirectory(assets)
add_dependencies(HelloTriangle CopyAssets)
add_dependencies(HelloTriangle CompileShaders)
add_dependencies(HelloTriangle CopyCubeMapImages)
add_dependencies(HelloTriangle assimp)
