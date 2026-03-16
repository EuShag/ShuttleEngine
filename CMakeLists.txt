cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(HelloTriangle)

add_executable(HelloTriangle
	"src/HelloTriangle.cpp"
	"src/Sdl/SdlLibrary/SdlLibrary.cpp" 
	"src/Sdl/SdlWindow/SdlWindow.cpp" 
	"src/VulkanInstanceBuilder/VulkanInstanceBuilder.cpp" 
	"src/VulkanDeviceBuilder/VulkanDeviceBuilder.cpp"
	"src/VulkanDebugger/VulkanDebugger.cpp" 
	"src/VulkanStructureChain/VulkanStructureChain.cpp" 
	"src/VulkanPhysicalDevicePicker/VulkanPhysilcalDevicePicker.cpp"
	"src/ImageLoader/Image.cpp"  
	"src/VulkanDeviceAllocator/VulkanDeviceAllocator.cpp" 
	"src/VulkanHelperFunctions/VulkanHelperFunctions.cpp" "src/VulkanModel/VulkanModel.cpp")

find_package(Vulkan REQUIRED)
set(SDL2Libraries ${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib ${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib)
target_include_directories(HelloTriangle PRIVATE ${Vulkan_INCLUDE_DIRS})

# add shader compilation step here if needed
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.vert.spv ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.frag.spv
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.frag.spv
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag
	COMMENT "Compiling shaders..."
)

add_custom_target(CompileShaders ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.vert.spv ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.frag.spv
)

add_dependencies(HelloTriangle CompileShaders)

target_link_libraries(HelloTriangle PRIVATE ${SDL2Libraries})
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")