cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
	# Ищем настоящий bash.exe в стандартных путях установки Git
	find_program(REAL_BASH_PATH
			NAMES bash
			PATHS
			"E:/Program Files/Git/bin"
			"E:/Program Files/Git/usr/bin"
			"E:/Program Files/Git/bin/bash.exe"
			NO_DEFAULT_PATH
	)
	if(REAL_BASH_PATH)
		# FORCE принудительно перезаписывает сломанный кэш CMake
		set(BASH_EXECUTABLE "${REAL_BASH_PATH}" CACHE FILEPATH "Forced real Bash path for KTX" FORCE)
		message(STATUS "[ShuttleEngine] Нашли настоящий Bash: ${BASH_EXECUTABLE}")
	else()
		message(WARNING "[ShuttleEngine] Настоящий Git Bash не найден! Сборка KTX может упасть.")
	endif()
endif()

message(STATUS "ImGui source dir: ${imgui_SOURCE_DIR}" )

project(ShuttleEngine)

find_package(Vulkan REQUIRED)
find_package(OpenMP REQUIRED)
message("Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")
set(SDL2Libraries
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib)

add_subdirectory(external/KTX-Software)
add_subdirectory(shaders)
add_subdirectory(img)
add_subdirectory(assets)
add_subdirectory(Engine)
add_subdirectory(AssetProcessor)
add_subdirectory(external/assimp)