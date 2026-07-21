#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inShadowCoord;

layout(location = 0) out vec4 outColor;

// Самплеры (набор 0)
layout(set = 0, binding = 0) uniform sampler materialSampler;
layout(set = 0, binding = 1) uniform sampler shadowSampler;

// Камера и Свет (набор 2)
layout(set = 2, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} camera;

struct DirectionalLightData {
    vec4 direction;
    vec4 color;
    mat4 lightSpaceMatrix;
};

layout(set = 2, binding = 1) uniform LightData {
    vec4 ambientColor;      // rgb: цвет, a: интенсивность
    uint lightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint padding;
} sceneLight;

layout(set = 2, binding = 2) readonly buffer DirectionalLightDatas {
    DirectionalLightData directionalLight[];
} directionalLights;

layout(set = 2, binding = 3) uniform texture2D shadowMap;

// Материал (набор 3)
layout(set = 3, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    float emissiveStrength;
    vec3 emissiveFactor;
} material;

layout(set = 3, binding = 1) uniform texture2D albedoMap;
layout(set = 3, binding = 2) uniform texture2D normalMap;
layout(set = 3, binding = 3) uniform texture2D ormMap;
layout(set = 3, binding = 4) uniform texture2D emissiveMap;
layout(set = 3, binding = 5) uniform texture2D heightMap;

// Environment (set = 4)

layout(set = 4, binding = 0) uniform textureCube skyboxMap;
layout(set = 4, binding = 1) uniform textureCube irradianceMap;
layout(set = 4, binding = 2) uniform textureCube radianceMap;
layout(set = 4, binding = 3) uniform texture2D brdfLut;

const float PI = 3.14159265359;


vec3 fresnelSchlickRoughness(
        float cosTheta,
        vec3 F0,
        float roughness)
{
    return F0 +
    (max(vec3(1.0 - roughness), F0) - F0) *
    pow(1.0 - cosTheta, 5.0);
}


// ----------- ТЕНИ (PCF) -----------
float calculateShadow(vec4 shadowCoord, vec3 geoNormal, vec3 L) {
    if (shadowCoord.w <= 0.0) return 1.0;
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.x > 1.0 || projCoords.x < 0.0 || projCoords.y > 1.0 || projCoords.y < 0.0 || projCoords.z > 1.0 || projCoords.z < 0.0) return 1.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(sampler2D(shadowMap, shadowSampler), 0).xy;
    float bias = max(0.0003 * (1.0 - dot(geoNormal, L)), 0.00005);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(sampler2D(shadowMap, shadowSampler), projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

// ----------- PBR МАТЕМАТИКА -----------
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 1. Текстуры
    // Сэмплим альбедо из UNORM-текстуры
    vec4 rawAlbedo = texture(sampler2D(albedoMap, materialSampler), inUv) * material.baseColorFactor;
    if (rawAlbedo.a < 0.5) discard;

    // ХАК: Вручную переводим цвет в Linear пространство. Текстура станет темнее,
    // но сохранит физическую чистоту для PBR-математики! [1.4]
    vec3 albedo = pow(rawAlbedo.rgb, vec3(2.2));

    vec3 orm = texture(sampler2D(ormMap, materialSampler), inUv).rgb;
    float ao = orm.r * material.occlusionStrength;
    float roughness = max(orm.g * material.roughnessFactor, 0.05); // Защита от нулевой шероховатости
    float metallic = orm.b * material.metallicFactor;

    // 2. Нормали (TBN)
    vec3 N = normalize(inNormal);
    vec3 WorldNormal = N;
    if (dot(inTangent.xyz, inTangent.xyz) > 1e-8 && abs(inTangent.w) > 1e-5) {
        // Читаем ТОЛЬКО R и G каналы (так как это BC5) и переводим из [0..1] в [-1..1]
        vec2 normalXY = texture(sampler2D(normalMap, materialSampler), inUv).rg * 2.0 - 1.0;
        normalXY.y = -normalXY.y;

        // Математически восстанавливаем Z-канал.
        // max(0.0, ...) защищает от получения отрицательного числа под корнем (NaN)
        // из-за погрешностей сжатия на краях геометрии
        float normalZ = sqrt(clamp(0.0, 1.0 - dot(normalXY, normalXY), 1.0));

        // Собираем полноценный вектор нормали в тангентном пространстве
        vec3 tangentNormal = vec3(normalXY, normalZ);

        // Строим ортонормированный базис TBN
        vec3 T = normalize(inTangent.xyz - N * dot(inTangent.xyz, N));
        vec3 B = cross(N, T) * inTangent.w; // Учитываем ориентацию бинормали
        mat3 TBN = mat3(T, B, N);

        // Переводим восстановленную нормаль в мировое пространство
        WorldNormal = normalize(TBN * tangentNormal);
    }

    vec3 V = normalize(camera.cameraPos - inWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // 3. Прямой свет (Lo)
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < sceneLight.lightCount; ++i) {
        vec3 L = normalize(directionalLights.directionalLight[i].direction.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = directionalLights.directionalLight[i].color.rgb * directionalLights.directionalLight[i].color.a;

        float NdotL = max(dot(WorldNormal, L), 0.0);
        if (NdotL > 0.0) {
            float shadow = calculateShadow(inShadowCoord, N, L); // N - геометрическая нормаль

            float NDF = DistributionGGX(WorldNormal, H, roughness);
            float G   = GeometrySmith(WorldNormal, V, L, roughness);
            vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(WorldNormal, V), 0.0) * NdotL + 0.0001;
            vec3 specular = numerator / denominator;

            Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
        }
    }

    float NdotV = max(dot(WorldNormal, V), 0.0);

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

    vec3 kS = F;

    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = texture(samplerCube(irradianceMap, materialSampler), WorldNormal).rgb;

    vec3 diffuseIBL = irradiance * albedo;

    vec3 R = reflect(-V, WorldNormal);

    const float MAX_REFLECTION_LOD = 5.0;

    vec3 prefilteredColor = textureLod(samplerCube(radianceMap, materialSampler), R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 brdf = texture(sampler2D(brdfLut, materialSampler), vec2(NdotV, roughness)).rg;

    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);


    vec3 ambient = (kD * diffuseIBL + specularIBL);


    // Если текстуры свечения пока нет, берем базовый вектор цвета и умножаем на силу интенсивности!
    vec3 finalEmissive = material.emissiveFactor.rgb * material.emissiveStrength;

    // ВНУТРИ main() ТВОЕГО ФРАГМЕНТНОГО ШЕЙДЕРА:
    // ВНУТРИ ТВОЕГО ФРАГМЕНТНОГО ШЕЙДЕРА:
    vec3 finalColor = ambient + Lo + finalEmissive;

    // ХАК ДЛЯ ВИРТУАЛЬНОЙ ДИАФРАГМЫ (ЭКСПОЗИЦИЯ ДВИЖКА):
    // Подкручивая этот float, ты можешь делать сцену темнее или светлее, как в настоящей камере!
    float exposure = 0.030;
    vec3 exposedColor = finalColor * exposure;

    // Тональное отображение ACES теперь применяем к EXPOSED цвету! [1.4]
    vec3 x = exposedColor;
    vec3 acesColor = clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);

    // Гамма-коррекция 2.2
    vec3 srgbColor = pow(acesColor, vec3(1.0 / 2.2));

    outColor = vec4(srgbColor, 1.0);
}