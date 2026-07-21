#version 460

layout(location = 0) out vec3 vDirection;

layout(set = 2, binding = 0) uniform CameraData {
	mat4 view;
	mat4 projection;
	vec4 cameraPosition;
} cameraData;

const vec3 kCubeVertices[36] = {
vec3(-1,-1,-1), vec3( 1,-1,-1), vec3( 1, 1,-1), vec3( 1, 1,-1), vec3(-1, 1,-1), vec3(-1,-1,-1),
vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1, 1, 1), vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1,-1, 1),
vec3(-1, 1, 1), vec3(-1, 1,-1), vec3(-1,-1,-1), vec3(-1,-1,-1), vec3(-1,-1, 1), vec3(-1, 1, 1),
vec3( 1, 1, 1), vec3( 1, 1,-1), vec3( 1,-1,-1), vec3( 1,-1,-1), vec3( 1,-1, 1), vec3( 1, 1, 1),
vec3(-1,-1,-1), vec3( 1,-1,-1), vec3( 1,-1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1), vec3(-1,-1,-1),
vec3(-1, 1,-1), vec3( 1, 1,-1), vec3( 1, 1, 1), vec3( 1, 1, 1), vec3(-1, 1, 1), vec3(-1, 1,-1)
};

void main() {
	vec3 position = kCubeVertices[gl_VertexIndex];

	// ИСПРАВЛЕНИЕ 1: Направление сэмплирования — это сама вершина куба в мировом пространстве!
	// Vulkan Cubemap ожидает именно чистый вектор направления из центра куба наружу.
	vDirection = position;

	// Вырезаем трансляцию (смещение) из матрицы view, оставляя только вращение
	mat4 viewWithoutTranslation = mat4(mat3(cameraData.view));

	// ИСПРАВЛЕНИЕ 2: Перемножаем канонично. Сначала вращаем куб в пространстве камеры, затем проецируем
	vec4 clipPos = cameraData.projection * viewWithoutTranslation * vec4(position, 1.0);

	// Оставляем твой крутой хак с xyww — он гарантирует, что Z всегда будет равен 1.0 (максимальная глубина)
	gl_Position = clipPos.xyww;
}
