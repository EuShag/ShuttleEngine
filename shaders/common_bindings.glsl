#ifndef SHUTTLE_COMMON_BINDINGS_GLSL
#define SHUTTLE_COMMON_BINDINGS_GLSL

// ============================================================
// Descriptor Sets
// ============================================================

#define SET_RENDERER 0
#define SET_ENVIRONMENT 1
#define SET_SCENE 2
#define SET_FRAME 3

// ============================================================
// Renderer Bindings
// ============================================================

#define RENDERER_MATERIAL_SAMPLER 0
#define RENDERER_SHADOW_SAMPLER 1
#define RENDERER_NEAREST_SAMPLER 2
#define RENDERER_BRDF_LUT_IMAGE 3

// ============================================================
// Environment Bindings
// ============================================================

#define ENV_SKYBOX_IMAGE 0
#define ENV_IRRADIANCE_IMAGE 1
#define ENV_RADIANCE_IMAGE 2

// ============================================================
// Scene Bindings
// ============================================================

#define SCENE_NODES 0
#define SCENE_NODE_LEVELS 1
#define SCENE_TRANSFORMS 2

#define SCENE_DRAWABLES 3

#define SCENE_POSITIONS 4
#define SCENE_ATTRIBUTES 5
#define SCENE_MESHES 6
#define SCENE_INDICES 7

#define SCENE_MATERIALS 8
#define SCENE_DIRECTIONAL_LIGHTS 9

#define SCENE_INFO 10
#define SCENE_TEXTURES 11

// ============================================================
// Frame Bindings
// ============================================================

#define FRAME_INFO 0
#define FRAME_FRUSTUM_PLANES 1

// ============================================================
// Directional Shadows
// ============================================================

#define FRAME_DIRECTIONAL_SHADOW_DATA 2

// ============================================================
// Scene Update
// ============================================================

#define FRAME_WORLD_TRANSFORMS 3

#define FRAME_MESH_RANGES 4
#define FRAME_INSTANCE_REMAP 5

// ============================================================
// Draw Generation
// ============================================================

#define FRAME_INDIRECT_COMMANDS 6
#define FRAME_DRAW_COUNT 7

// ============================================================
// Images
// ============================================================

#define FRAME_DEPTH_IMAGE 8
#define FRAME_DIRECTIONAL_SHADOW_MAP_IMAGE 9

#endif