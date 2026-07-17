#pragma once

#include <glm/glm.hpp> // Для glm::vec3, glm::vec4, glm::mat4 (в MaterialInfo, ModelData)

// Макрос для выравнивания структур (поскольку glm::vec4 и glm::mat4 уже 16-байтовые)
#ifndef SHUTTLE_ALIGN_16
#define SHUTTLE_ALIGN_16 alignas(16)
#endif

namespace shuttle_engine::format {

    struct alignas(16) BlobHeader {
        // =========================================================================
        // 1. ИДЕНТИФИКАЦИЯ И РАЗМЕР ФАЙЛА (Размер: 16 байт)
        // =========================================================================
        char     magic[4];               // [000..003] Всегда "BLOB" (валидация файла)
        uint32_t version;                // [004..007] Версия формата (например, 1, 2, 3...)
        uint64_t totalFileSize;          // [008..015] Полный размер файла на диске (в байтах)

        // =========================================================================
        // 2. БЛОК ТЕКСТУР И МАТЕРИАЛОВ (Размер: 32 байта)
        // =========================================================================
        uint64_t textureTableOffset;     // [016..023] Смещение до массива TextureMetaData[]
        uint32_t textureCount;           // [024..027] Количество текстур в сцене
        uint32_t padding0;               // [028..031] Выравнивание
        uint64_t materialTableOffset;    // [032..039] Смещение до массива MaterialConstants[]
        uint32_t materialCount;          // [040..043] Количество материалов
        uint32_t padding1;               // [044..047] Выравнивание

        // =========================================================================
        // 3. БЛОК ГЕОМЕТРИИ (Размер: 16 байт)
        // =========================================================================
        uint64_t meshTableOffset;        // [048..055] Смещение до массива MeshHeader[]
        uint32_t meshCount;              // [056..059] Количество уникальных мешей
        uint32_t padding2;               // [060..063] Выравнивание

        // =========================================================================
        // 4. БЛОК СКЕЛЕТОВ И СВЯЗАННЫХ АНИМАЦИЙ (Размер: 32 байта)
        // =========================================================================
        uint64_t skeletonTableOffset;    // [064..071] Смещение до массива SkeletonData[]
        uint32_t skeletonCount;          // [072..075] Количество скелетов на сцене
        uint32_t padding3;               // [076..079] Выравнивание
        uint64_t boneChannelTableOffset; // [080..087] Смещение до массива BoneChannel[]
        uint32_t boneChannelCount;       // [088..091] Количество скелетных каналов
        uint32_t padding4;               // [092..095] Выравнивание

        // =========================================================================
        // 5. БЛОК МОРФИНГА / БЛЕНДШЕЙПОВ ЛИЦА (Размер: 32 байта)
        // =========================================================================
        uint64_t morphTargetTableOffset; // [096..103] Смещение до массива MorphTargetHeader[]
        uint32_t morphTargetCount;       // [104..107] Количество выражений/форм лица
        uint32_t padding5;               // [108..111] Выравнивание
        uint64_t morphChannelTableOffset;// [112..119] Смещение до массива MorphChannel[]
        uint32_t morphChannelCount;      // [120..123] Количество каналов морфинга
        uint32_t padding6;               // [124..127] Выравнивание

        // =========================================================================
        // 6. БЛОК ДИНАМИЧЕСКИХ МАТЕРИАЛОВ (Размер: 32 байта)
        // =========================================================================
        uint64_t matPropertyTableOffset; // [128..135] Смещение до массива MaterialPropertyDesc[]
        uint32_t matPropertyCount;       // [136..139] Количество анимируемых свойств материалов
        uint32_t padding7;               // [140..143] Выравнивание
        uint64_t matChannelTableOffset;  // [144..151] Смещение до массива MaterialPropertyChannel[]
        uint32_t matChannelCount;        // [152..155] Количество каналов материалов
        uint32_t padding8;               // [156..159] Выравнивание

        // =========================================================================
        // 7. СИСТЕМНЫЕ ССЫЛКИ И ОФФСЕТ КЛИПОВ (Размер: 32 байта)
        // =========================================================================
        uint64_t clipTableOffset;        // [160..167] Смещение до общего массива клипов AnimationClip[]
        uint32_t clipCount;              // [168..171] Количество клипов (Run, Jump, Idle...)
        uint32_t padding9;               // [172..175] Выравнивание
        uint64_t sceneGraphOffset;       // [176..183] Смещение до плоского массива SceneNode[] на диске
        uint32_t sceneNodeCount;         // [184..187] Общее количество нод в графе сцены
        uint32_t paddingBlob;            // [188..191] Выравнивание заголовка блока

        // =========================================================================
        // 8. НАЧАЛО СЫРЫХ БИНАРНЫХ ДАННЫХ (Размер: 16 байт)
        // =========================================================================
        uint64_t bulkDataOffset;         // [192..199] Физическое начало бинарного блока данных (Bulk Data)
        uint64_t reserved;               // [200..207] Запасное поле на будущее
    };


    // Жесткий контроль структуры компилятором
    static_assert(sizeof(BlobHeader) == 208, "Размер BlobHeader должен быть ровно 208 байт!");
    static_assert(offsetof(BlobHeader, bulkDataOffset) == 192, "Смещение bulkDataOffset нарушено!");

    // --- Метаданные материалов ---
    enum class MaterialPipelineFlagBits : uint32_t {
        eNone = 0,
        eAlphaTest = 1 << 0,   // Материал требует альфа-теста
        eTransparent = 1 << 1, // Материал прозрачный (alpha blending)
        eDoubleSided = 1 << 2, // Отрисовывать обе стороны полигона
        eUnlit = 1 << 3        // Материал не освещается (для UI, спецэффектов)
    };

    struct alignas(16) MaterialInfo {
        glm::vec4 baseColorFactor;
        glm::vec4 emissiveFactor;
        float metallicFactor;
        float roughnessFactor;
        float occlusionStrength;
        float emissiveStrength;
        float alphaCutoff;

        // Индексы текстур в глобальном массиве TextureMetaData (-1 если текстуры нет)
        int32_t albedoTexIdx;
        int32_t normalTexIdx;
        int32_t ormTexIdx;
        int32_t emissiveTexIdx;
        int32_t heightTexIdx; // Для карты высот (Parallax Occlusion Mapping)

        uint32_t pipelineFlags; // Флаги из MaterialPipelineFlagBits
        uint32_t padding[1];    // Заполнитель
    };

    // --- Метаданные текстур ---
    struct alignas(16) TextureMetaData {
        uint64_t textureOffset;

        uint32_t format; // VkFormat (например, VK_FORMAT_BC7_SRGB_BLOCK)
        uint32_t width;
        uint32_t height;
        uint32_t mipCount;
        uint32_t numLayers;
        uint32_t isCubemap; // 1 если кубическая текстура, 0 если 2D/3D
    };

    // --- Метаданные геометрии (Меши и LOD'ы) ---

    enum class MeshAttributeFlags : uint32_t {
        eNone = 0,
        eHasNormal = 1 << 0,
        eHasTangent = 1 << 1,
        eHasUV = 1 << 2,
        eHasColor = 1 << 3,
        eHasSkinning = 1 << 4 // Есть данные костей/весов
    };

    // Компактная внутренняя структура (весит ровно 40 байт)
    struct MeshLODData {
        uint32_t indexCount;             // [0..3]
        uint32_t firstIndex;             // [4..7]
        int32_t  vertexOffset;           // [8..11]
        int32_t  materialIndexOverride;  // [12..15] (-1 если дефолт)

        glm::vec3 minBounds;             // [16..27] AABB конкретного ЛОДа
        float    lodScreenSizeThreshold; // [28..31] Порог переключения (размер на экране)
        glm::vec3 maxBounds;             // [32..43]
        float    padding;                // [44..47] Выравнивание под 16 байт
    };

    struct alignas(16) MeshHeader {
        // 1. Физические GPU-адреса геометрии (32 байта)
        uint64_t positionBufferAddress;    // [0..7]
        uint64_t attributeBufferAddress;   // [8..15]
        uint64_t indexBufferAddress;       // [16..23]
        uint64_t boneIdsWeightsDataOffset; // [24..31]

        // 2. Общие свойства меша (16 байт)
        uint32_t lodCount;                 // [32..35] Реальное кол-во ЛОДов (от 1 до 4)
        int32_t  defaultMaterialIndex;     // [36..39]
        uint32_t attributeFlags;           // [40..43]
        uint32_t explicitPadding;          // [44..47]

        // 3. ВСТРОЕННЫЙ МАССИВ ЛОДОВ (4 * 48 байт = 192 байта)
        // Начинается ровно со смещения 48 (кратно 16)
        MeshLODData lods[4];               // [48..239]

        // 4. Финальный хвостовой заполнитель структуры до красивых 256 байт
        uint32_t finalPadding[4];          // [240..255]
    };
    static_assert(sizeof(MeshHeader) == 256, "Размер MeshHeader должен быть ровно 256 байт!");

    // --- Анимационные структуры (для будущих симуляций персонажей и объектов) ---

    // =============================================================================
    // 1. СТРУКТУРА СКЕЛЕТА (Размер: ровно 16 байт)
    // =============================================================================
    struct alignas(16) SkeletonData {
        uint32_t boneOffset;       // Смещение первого BoneData в глобальном массиве костей
        uint32_t boneCount;        // Количество костей в этом скелете
        int32_t  rootBoneIndex;    // Индекс корневой кости (-1 если нет)
        uint32_t explicitPadding;  // Явный паддинг, закрывающий 16-байтовую линию
    };
    static_assert(sizeof(SkeletonData) == 16, "Размер SkeletonData должен быть ровно 16 байт!");

    // =============================================================================
    // 2. ДАННЫЕ КОСТИ (Размер: ровно 80 байт)
    // =============================================================================
    struct alignas(16) BoneData {
        glm::mat4 invBindMatrix;      // Обратная матрица связывания для скиннинга (64 байта)
        int32_t   parentIndex;        // Индекс родительской кости (-1 если нет, 4 байта)
        uint32_t  explicitPadding[3]; // Явный паддинг (12 байт). Добивает структуру до 80 байт (кратно 16)
    };
    static_assert(sizeof(BoneData) == 80, "Размер BoneData должен быть ровно 80 байт!");

    // =============================================================================
    // 3. КЛИП АНИМАЦИИ (Размер: ровно 16 байт)
    // =============================================================================
    // Имя клипа хранится во внешнем строковом пуле (String Pool), структура ультра-легкая
    struct alignas(16) AnimationClip {
        // [0..3] Длительность всей анимации в секундах
        float    duration;

        // [4..11] Скелетные треки костей
        uint32_t firstBoneChannelIdx;
        uint32_t boneChannelCount;

        // [12..19] Треки морфинга (мимики лица)
        uint32_t firstMorphChannelIdx;
        uint32_t morphChannelCount;

        // [20..27] Треки изменения параметров материалов
        uint32_t firstMatChannelIdx;
        uint32_t matChannelCount;

        // [28..31] Явный паддинг, заполняющий структуру ровно до 32 байт (кратно 16)
        uint32_t explicitPadding;
    };
    // Жесткая проверка компилятора
    static_assert(sizeof(AnimationClip) == 32, "Размер AnimationClip должен быть ровно 32 байта!");


    // =============================================================================
    // 4. КАНАЛ АНИМАЦИИ (Размер: ровно 32 байта)
    // =============================================================================
    struct alignas(16) AnimationChannel {
        uint32_t targetNodeIndex;  // Индекс ноды/кости, которую анимируем
        uint32_t pathType;         // 0: translation, 1: rotation (quat), 2: scale
        uint32_t samplerIndex;     // Индекс AnimationSampler (на будущее)
        uint32_t keyframeOffset;   // Смещение начала трека ОДНОВРЕМЕННО в буфере Times и буфере Values
        uint32_t keyframeCount;    // Количество ключевых кадров в этом треке
        uint32_t explicitPadding[3]; // Явный паддинг (12 байт). Закрывает структуру на отметке 32 байта
    };
    static_assert(sizeof(AnimationChannel) == 32, "Размер AnimationChannel должен быть ровно 32 байта!");

    struct alignas(16) BoneChannel {
        uint32_t boneIndex;          // ID кости в скелете
        uint32_t pathType;           // Strictly: 0 - Translation, 1 - Rotation, 2 - Scale
        uint32_t keyframeOffset;     // Оффсет в массивах скелетных кадров
        uint32_t keyframeCount;
    }; // Размер: 16 байт. Никакого мусора!

    struct alignas(16) MorphChannel {
        uint32_t morphTargetId;      // ID блендшейпа (например, "улыбка")
        uint32_t keyframeOffset;     // Оффсет в массивах float (тайминги и веса лежат плотно)
        uint32_t keyframeCount;
        uint32_t padding;
    }; // Размер: 16 байт. Кадр весит ВСЕГО 4 байта (float) вместо 16 байт!

    struct alignas(16) MaterialPropertyChannel {
        uint32_t materialId;         // Какой материал анимируем
        uint32_t propertyHash;       // Хэш имени параметра (например, "baseColor")
        uint32_t keyframeOffset;     // Оффсет в буфере vec4 значений
        uint32_t keyframeCount;
    }; // Размер: 16 байт.



    // =============================================================================
    // 5. КЛЮЧЕВОЙ КАДР АНИМАЦИИ — ОПТИМИЗИРОВАННЫЙ (Размер: ровно 16 байт)
    // =============================================================================
    // В бинарном Blob-файле и на GPU эти данные хранятся в двух параллельных буферах:
    // 1. Буфер Таймингов: float globalKeyframeTimes[] (4 байта на кадр, без паддингов)
    // 2. Буфер Значений:  AnimationKeyframeValue globalKeyframeValues[] (16 байт на кадр)
    struct alignas(16) AnimationKeyframeValue {
        glm::vec4 value; // vec4 для translation (xyz), scale (xyz) или rotation (xyzw кватернион)
    };
    static_assert(sizeof(AnimationKeyframeValue) == 16, "Размер кадра сокращен до 16 байт!");

    struct alignas(16) DirectionalLight {
        glm::vec3 direction;        // [0..11]  Направление света (x, y, z)
        float     intensity;        // [12..15] Интенсивность (яркость) света

        glm::vec3 color;            // [16..27] Цвет источника (R, G, B)
        uint32_t  castShadows;      // [28..31] Флаг: 1 — генерирует тени, 0 — нет
    };
    static_assert(sizeof(DirectionalLight) == 32, "Размер DirectionalLight должен быть ровно 32 байта!");

    struct alignas(16) PointLight {
        // x, y, z — позиция в мире, w — максимальный радиус действия (Influence Radius)
        glm::vec4 positionAndRadius; // [0..15]  16-байтовое выравнивание

        glm::vec3 color;            // [16..27] Цвет источника (R, G, B)
        float     intensity;        // [28..31] Интенсивность (яркость)
    };
    static_assert(sizeof(PointLight) == 32, "Размер PointLight должен быть ровно 32 байта!");

    struct alignas(16) SpotLight {
        glm::vec4 positionAndRadius;     // [0..15]  x,y,z - pos, w - radius
        glm::vec4 directionAndIntensity; // [16..31] x,y,z - dir, w - intensity

        glm::vec3 color;                 // [32..43] Цвет (R, G, B)
        float     innerCutOff;           // [44..47] cos(внутреннего угла)

        float     outerCutOff;           // [48..51] cos(внешнего угла)
        uint32_t  castShadows;           // [52..55] Флаг теней
        uint32_t  explicitPadding[2];    // [56..63] Ручной паддинг. Структура ровно 64 байта.
    };
    static_assert(sizeof(SpotLight) == 64, "Размер SpotLight должен быть ровно 64 байта!");

    struct alignas(16) MorphTargetHeader {
        uint32_t firstDeltaGlobalIdx; // [0..3]   Смещение первого MorphVertexDelta в глобальном буфере
        uint32_t deltaCount;          // [4..7]   Сколько ВСЕГО вершин деформирует эта цель
        uint32_t targetNameHash;      // [8..11]  Хэш имени цели (например, для связки с липсинк-системами)
        uint32_t explicitPadding;     // [12..15] Выравнивание первой 16-байтовой линии

        // Максимальный радиус и габариты деформации (нужны для точного Frustum Culling объекта)
        glm::vec3 maxPositionDelta;   // [16..27] На какую максимальную величину могут сдвинуться вершины
        uint32_t  explicitPadding2;   // [28..31] Добиваем структуру до 32 байт (std430 / alignas(16))
    };
    static_assert(sizeof(MorphTargetHeader) == 32, "Размер MorphTargetHeader должен быть ровно 32 байта!");

    struct alignas(16) MorphVertexDelta {
        glm::vec3 positionDelta;      // [0..11]  Вектор смещения позиции вершины (x, y, z)
        uint32_t  vertexIndex;        // [12..15] Оригинальный индекс вершины в базовом буфере меша,
        // которую нужно сдвинуть.
    };
    static_assert(sizeof(MorphVertexDelta) == 16, "Размер MorphVertexDelta должен быть ровно 16 байт!");

    struct alignas(16) MaterialPropertyDesc {
        uint32_t propertyHash;        // Хэш имени переменной в шейдере (например, "u_EmissionColor")
        uint16_t elementCount;        // Сколько компонентов (1 для float, 3 для vec3, 4 для vec4)
        uint16_t offsetInMaterial;    // Смещение параметра в байтах внутри структуры материала
        uint32_t explicitPadding;
    };
    static_assert(sizeof(MaterialPropertyDesc) == 16, "Размер MaterialPropertyDesc должен быть ровно 16 байт!");

    struct alignas(16) SceneNode {
        glm::vec3 localTranslation;    // [0..11]
        uint32_t  parentIndex;         // [12..15]

        glm::vec4 localRotationQuat;   // [16..31]

        glm::vec3 localScale;          // [32..43]
        uint32_t  instanceIndex;       // [44..47] Изменили на uint32_t ради идеального выравнивания
    };
    static_assert(sizeof(SceneNode) == 48, "Размер SceneNode должен быть ровно 48 байт!");



} // namespace shuttle_engine::format