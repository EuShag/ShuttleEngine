cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(HelloTriangle)

add_executable(HelloTriangle
	"HelloTriangle.cpp"
	"src/Sdl/SdlLibrary/SdlLibrary.cpp" 
	"src/Sdl/SdlWindow/SdlWindow.cpp" 
	"src/VulkanInstanceBuilder/VulkanInstanceBuilder.cpp" 
	"src/VulkanDeviceBuilder/VulkanDeviceBuilder.cpp"
	"src/VulkanDebugger/VulkanDebugger.cpp" 
	"src/VulkanStructureChain/VulkanStructureChain.cpp" 
	"src/VulkanPhysicalDevicePicker/VulkanPhysilcalDevicePicker.cpp"
	"src/ImageLoader/Image.cpp"  
	"src/VulkanDeviceAllocator/VulkanDeviceAllocator.cpp" 
	"src/VulkanHelperFunctions/VulkanHelperFunctions.cpp" 
	"src/VulkanModel/VulkanModel.cpp" 
	"src/VulkanCopyManager/VulkanCopyManager.cpp" 
	"src/VulkanApplication/VulkanApplication.cpp" 
	"src/Camera/Camera.cpp" "src/Skybox/Skybox.cpp")

find_package(Vulkan REQUIRED)
message("Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")
set(SDL2Libraries
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib)

target_include_directories(HelloTriangle PRIVATE ${Vulkan_INCLUDE_DIRS})

target_link_libraries(HelloTriangle PRIVATE ${SDL2Libraries})
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/VMA/include")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")

add_subdirectory(shaders)
add_dependencies(HelloTriangle CompileShaders)

