#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inShadowCoord; // Оставляем для совместимости, но будем считать точнее

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler materialSampler;
layout(set = 0, binding = 1) uniform sampler shadowSampler;

layout(set = 2, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec3 cameraPos;
} camera;

struct DirectionalLightData {
    vec4 direction;
    vec4 color;
    mat4 lightSpaceMatrix;
};

layout(set = 2, binding = 1) uniform LightData {
    vec4 ambientColor;
    uint lightCount;
    uint pointLightCount;
    uint spottLightCount;
    uint padding;
} sceneLight;

layout(set = 2, binding = 2) readonly buffer DirectionalLightDatas{
    DirectionalLightData directionalLight[];
} directionalLights;

layout(set = 2, binding = 3) uniform texture2D shadowMap;

layout(set = 3, binding = 0) uniform MaterialUBO {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
} material;

layout(set = 3, binding = 1) uniform texture2D albedoMap;
layout(set = 3, binding = 2) uniform texture2D normalMap;
layout(set = 3, binding = 3) uniform texture2D ormMap;
layout(set = 3, binding = 4) uniform texture2D emissiveMap;
layout(set = 3, binding = 5) uniform texture2D heightMap;

const float PI = 3.14159265359;

// ИСПРАВЛЕНО: Теперь функция корректно использует переданный параметр shadowCoord и гео-нормаль
float calculateShadow(vec4 shadowCoord, vec3 geoNormal, vec3 L) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Если объект полностью за пределами теневой матрицы — он освещен (1.0)
    if (projCoords.x > 1.0 || projCoords.x < 0.0 ||
        projCoords.y > 1.0 || projCoords.y < 0.0 ||
        projCoords.z > 1.0 || projCoords.z < 0.0)
    {
        return 1.0;
    }

    vec2 ndcDist = min(projCoords.xy, vec2(1.0) - projCoords.xy);
    float distToEdge = min(ndcDist.x, ndcDist.y);

    float fadeStart = 0.1;
    float fadeFactor = clamp(distToEdge / fadeStart, 0.0, 1.0);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(sampler2D(shadowMap, shadowSampler), 0);

    // ИСПРАВЛЕНО: Используем чистую геометрическую нормаль для расчета depth bias
    float bias = max(0.0003 * (1.0 - dot(geoNormal, L)), 0.00005);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(sampler2D(shadowMap, shadowSampler), projCoords.xy + vec2(x, y) * texelSize).r;
            // ИСПРАВЛЕНО: Сравниваем с использованием переданного смещения bias
            shadow += (projCoords.z - bias) > pcfDepth ? 0.0 : 1.0;
        }
    }

    float rawShadowFactor = shadow / 9.0;

    return mix(1.0, rawShadowFactor, fadeFactor);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) { float a = roughness*roughness; float a2 = a*a; float NdotH = max(dot(N, H), 0.0); float denom = (NdotH*NdotH*(a2 - 1.0) + 1.0); return a2 / (PI * denom * denom); }
float GeometrySchlickGGX(float NdotV, float roughness) { float r = (roughness + 1.0); float k = (r*r)/8.0; return NdotV / (NdotV*(1.0-k) + k); }
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) { return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness); }
vec3 fresnelSchlick(float cosTheta, vec3 F0) { return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0); }

void main() {
    vec4 albedoTex = texture(sampler2D(albedoMap, materialSampler), inUv) * material.baseColorFactor;
    if (albedoTex.a < material.alphaCutoff) discard;
    vec3 albedo = albedoTex.rgb;

    vec3 orm = texture(sampler2D(ormMap, materialSampler), inUv).rgb;
    float ao = orm.r;
    float roughness = orm.g * material.roughnessFactor;
    float metallic = orm.b * material.metallicFactor;

    // Нормаль из карты нормалей (для PBR освещения)
    vec3 tangentNormal = texture(sampler2D(normalMap, materialSampler), inUv).xyz * 2.0 - 1.0;
    vec3 N = normalize(inNormal); // Геометрическая нормаль
    vec3 T = normalize(inTangent.xyz - N * dot(inTangent.xyz, N));
    vec3 B = normalize(cross(N, T) * inTangent.w);
    mat3 TBN = mat3(T, B, N);
    vec3 normal = normalize(TBN * tangentNormal);

    vec3 V = normalize(camera.cameraPos - inWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < sceneLight.lightCount; ++i) {
        vec3 L = normalize(directionalLights.directionalLight[i].direction.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = directionalLights.directionalLight[i].color.rgb * directionalLights.directionalLight[i].color.a;

        float NDF = DistributionGGX(normal, H, roughness);
        float G   = GeometrySmith(normal, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (NDF * G * F) / (4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        float shadow = 1.0;
        if (i == 0) {
            // =========================================================================
            // НОВАЯ ЛОГИКА: NORMAL OFFSET BIAS
            // =========================================================================
            // 1. Считаем угол между светом и геометрической нормалью поверхности
            float cosTheta = clamp(dot(N, L), 0.0, 1.0);
            float slopeScale = sqrt(1.0 - cosTheta * cosTheta); // Синус угла наклона

            // 2. Смещаем мировую позицию фрагмента ВДОЛЬ нормали наружу.
            // Величина смещения зависит от угла: на крутых склонах баллона сдвигаем сильнее.
            // Примечание: 0.005f - начальное смещение, подбирается под масштаб сцены
            float normalOffsetStrength = 0.004;
            vec3 biasedWorldPos = inWorldPos + N * (normalOffsetStrength * slopeScale);

            // 3. Проецируем уже СДВИГНУТУЮ координату в пространство света
            vec4 biasedShadowCoord = directionalLights.directionalLight[i].lightSpaceMatrix * vec4(biasedWorldPos, 1.0);

            // 4. Вызываем расчет теней, передавая геометрическую нормаль N
            shadow = calculateShadow(biasedShadowCoord, N, L);
            // =========================================================================
        }

        Lo += (kD * albedo / PI + specular) * radiance * max(dot(normal, L), 0.0) * shadow;
    }

    vec3 ambient = sceneLight.ambientColor.rgb * sceneLight.ambientColor.a * albedo * ao;
    vec3 color = ambient + Lo + (texture(sampler2D(emissiveMap, materialSampler), inUv).rgb * material.emissiveFactor.rgb);

    outColor = vec4(pow(color / (color + vec3(1.0)), vec3(1.0/2.2)), 1.0);
}