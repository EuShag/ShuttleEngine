#ifndef SHUTTLE_COMMON_DESCRIPTOR_HEAP_GLSL
#define SHUTTLE_COMMON_DESCRIPTOR_HEAP_GLSL

#extension GL_EXT_nonuniform_qualifier : require

// ============================================================
// Descriptor heap binding fallback
//
// Matches C++:
//
// enum class DescriptorHeapBinding : uint32_t
// {
//     Sampler = 0,
//     Texture = 1,
//     StorageImage = 2,
//     UniformTexelBuffer = 3,
//     StorageTexelBuffer = 4
// };
//
// ============================================================

#ifndef DESCRIPTOR_SET
#define DESCRIPTOR_SET 0
#endif

#ifndef SAMPLER
#define SAMPLER 0
#endif

#ifndef TEXTURE
#define TEXTURE 1
#endif

#ifndef STORAGE_IMAGE
#define STORAGE_IMAGE 2
#endif

#ifndef UNIFORM_TEXEL_BUFFER
#define UNIFORM_TEXEL_BUFFER 3
#endif

#ifndef STORAGE_TEXEL_BUFFER
#define STORAGE_TEXEL_BUFFER 4
#endif

// ============================================================
// Sampled images
//
// All of these alias the same Vulkan binding:
//
// binding = TEXTURE
// descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
//
// The actual dimensionality must match the VkImageView type.
// ============================================================

layout(set = DESCRIPTOR_SET, binding = TEXTURE)
uniform texture2D heapTexture2D[];

layout(set = DESCRIPTOR_SET, binding = TEXTURE)
uniform texture2DArray heapTexture2DArray[];

layout(set = DESCRIPTOR_SET, binding = TEXTURE)
uniform textureCube heapTextureCube[];

layout(set = DESCRIPTOR_SET, binding = TEXTURE)
uniform textureCubeArray heapTextureCubeArray[];

layout(set = DESCRIPTOR_SET, binding = TEXTURE)
uniform texture3D heapTexture3D[];

// ============================================================
// Samplers
//
// binding = SAMPLER
// descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER
// ============================================================

layout(set = DESCRIPTOR_SET, binding = SAMPLER)
uniform sampler heapSampler[];

// ============================================================
// Storage images
//
// All of these alias the same Vulkan binding:
//
// binding = STORAGE_IMAGE
// descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
//
// IMPORTANT:
// The format qualifier must match compatible image usage.
// If you use different storage image formats, prefer declaring
// storage images in pass-specific shader files instead of here.
// ============================================================

#ifdef SHUTTLE_DECLARE_STORAGE_IMAGES_RGBA16F

layout(rgba16f, set = DESCRIPTOR_SET, binding = STORAGE_IMAGE)
uniform image2D heapStorageImage2D[];

layout(rgba16f, set = DESCRIPTOR_SET, binding = STORAGE_IMAGE)
uniform image2DArray heapStorageImage2DArray[];

layout(rgba16f, set = DESCRIPTOR_SET, binding = STORAGE_IMAGE)
uniform imageCube heapStorageImageCube[];

layout(rgba16f, set = DESCRIPTOR_SET, binding = STORAGE_IMAGE)
uniform imageCubeArray heapStorageImageCubeArray[];

layout(rgba16f, set = DESCRIPTOR_SET, binding = STORAGE_IMAGE)
uniform image3D heapStorageImage3D[];

#endif

// ============================================================
// Texel buffers
// ============================================================

layout(set = DESCRIPTOR_SET, binding = UNIFORM_TEXEL_BUFFER)
uniform samplerBuffer heapUniformTexelBuffer[];

#ifdef SHUTTLE_DECLARE_STORAGE_TEXEL_BUFFERS_RGBA32F

layout(rgba32f, set = DESCRIPTOR_SET, binding = STORAGE_TEXEL_BUFFER)
uniform imageBuffer heapStorageTexelBuffer[];

#endif

// ============================================================
// Sample helpers
// ============================================================

vec4 sampleHeapTexture2D(
        uint textureIndex,
        uint samplerIndex,
        vec2 uv,
        vec4 fallbackValue)
{
    if (textureIndex == 0xffffffffu)
    {
        return fallbackValue;
    }

    return texture(
            sampler2D(
                    heapTexture2D[nonuniformEXT(textureIndex)],
                    heapSampler[nonuniformEXT(samplerIndex)]),
            uv);
}

vec4 sampleHeapTexture2DArray(
        uint textureIndex,
        uint samplerIndex,
        vec3 uvw,
        vec4 fallbackValue)
{
    if (textureIndex == 0xffffffffu)
    {
        return fallbackValue;
    }

    return texture(
            sampler2DArray(
                    heapTexture2DArray[nonuniformEXT(textureIndex)],
                    heapSampler[nonuniformEXT(samplerIndex)]),
            uvw);
}

vec3 sampleHeapTextureCube(
        uint textureIndex,
        uint samplerIndex,
        vec3 direction,
        vec3 fallbackValue)
{
    if (textureIndex == 0xffffffffu)
    {
        return fallbackValue;
    }

    return texture(
            samplerCube(
                    heapTextureCube[nonuniformEXT(textureIndex)],
                    heapSampler[nonuniformEXT(samplerIndex)]),
            direction).rgb;
}

vec3 sampleHeapTextureCubeLod(
        uint textureIndex,
        uint samplerIndex,
        vec3 direction,
        float lod,
        vec3 fallbackValue)
{
    if (textureIndex == 0xffffffffu)
    {
        return fallbackValue;
    }

    return textureLod(
            samplerCube(
                    heapTextureCube[nonuniformEXT(textureIndex)],
                    heapSampler[nonuniformEXT(samplerIndex)]),
            direction,
            lod).rgb;
}

vec4 sampleHeapTexture3D(
        uint textureIndex,
        uint samplerIndex,
        vec3 uvw,
        vec4 fallbackValue)
{
    if (textureIndex == 0xffffffffu)
    {
        return fallbackValue;
    }

    return texture(
            sampler3D(
                    heapTexture3D[nonuniformEXT(textureIndex)],
                    heapSampler[nonuniformEXT(samplerIndex)]),
            uvw);
}

#endif // SHUTTLE_COMMON_DESCRIPTOR_HEAP_GLSL