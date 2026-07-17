# add shader compilation step here if needed
add_custom_command(
	OUTPUT
		${CMAKE_CURRENT_BINARY_DIR}/skybox.vert.spv
		${CMAKE_CURRENT_BINARY_DIR}/skybox.frag.spv
		${CMAKE_CURRENT_BINARY_DIR}/simpleModel.vert.spv
		${CMAKE_CURRENT_BINARY_DIR}/simpleModel.frag.spv
		${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
		${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv
		${CMAKE_CURRENT_BINARY_DIR}/shadow.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/skybox.vert -o ${CMAKE_CURRENT_BINARY_DIR}/skybox.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/skybox.frag -o ${CMAKE_CURRENT_BINARY_DIR}/skybox.frag.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/simpleModel.vert -o ${CMAKE_CURRENT_BINARY_DIR}/simpleModel.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/simpleModel.frag -o ${CMAKE_CURRENT_BINARY_DIR}/simpleModel.frag.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/pbr.vert -o ${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/pbr.frag -o ${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/shadow.vert -o ${CMAKE_CURRENT_BINARY_DIR}/shadow.vert.spv
	DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/skybox.vert
		${CMAKE_CURRENT_SOURCE_DIR}/skybox.frag
		${CMAKE_CURRENT_SOURCE_DIR}/simpleModel.vert
		${CMAKE_CURRENT_SOURCE_DIR}/simpleModel.frag
		${CMAKE_CURRENT_SOURCE_DIR}/pbr.vert
		${CMAKE_CURRENT_SOURCE_DIR}/pbr.frag
		${CMAKE_CURRENT_SOURCE_DIR}/shadow.vert
	COMMENT "Compiling shaders..."
)

add_custom_target(CompileShaders ALL
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/skybox.vert.spv
		${CMAKE_CURRENT_BINARY_DIR}/skybox.frag.spv
		${CMAKE_CURRENT_BINARY_DIR}/simpleModel.vert.spv
		${CMAKE_CURRENT_BINARY_DIR}/simpleModel.frag.spv
		${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
		${CMAKE_CURRENT_BINARY_DIR}/pbr.frag.spv
		${CMAKE_CURRENT_BINARY_DIR}/shadow.vert.spv
)