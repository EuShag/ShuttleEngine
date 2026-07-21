    #pragma once

    #include <glm/glm.hpp> // Для glm::vec3, glm::vec4, glm::mat4 (в MaterialInfo, ModelData)

    // Макрос для выравнивания структур (поскольку glm::vec4 и glm::mat4 уже 16-байтовые)
    #ifndef SHUTTLE_ALIGN_16
    #define SHUTTLE_ALIGN_16 alignas(16)
    #endif

    namespace shuttle_engine::format {

        struct alignas(16) PositionAttribute {
            glm::vec4 position;
        };

        // =============================================================================
        // ГЛОБАЛЬНЫЕ КОНСТАНТЫ И СОГЛАШЕНИЯ
        // =============================================================================
        // Sentinel значения для индексов (означают "нет ссылки"):
        // - uint32_t индексы:  0xFFFFFFFF означает отсутствие ссылки
        // - int32_t индексы:   -1 означает отсутствие ссылки
        // Для совместимости рекомендуется использовать явные -1 или константы
        constexpr uint32_t INVALID_INDEX_U32 = 0xFFFFFFFFu;
        constexpr int32_t INVALID_INDEX_I32 = -1;
        constexpr uint32_t INVALID_HASH = 0u;
        constexpr uint64_t INVALID_OFFSET = 0u;

        struct alignas(16) BlobHeader {
            // [000..015] Идентификация и размер
            char     magic[4];               // "BLOB"
            uint32_t version;                // Версия 1
            uint64_t totalFileSize;          // Размер файла

            // [016..047] Текстуры и Материалы
            uint64_t textureTableOffset;
            uint32_t textureCount, padding0;
            uint64_t materialTableOffset;
            uint32_t materialCount, padding1;

            // [048..063] Геометрия
            uint64_t meshTableOffset;
            uint32_t meshCount, padding2;

            // [064..127] Скелеты, Анимации, Морфинг
            uint64_t skeletonTableOffset;
            uint32_t skeletonCount, padding3;
            uint64_t boneChannelTableOffset;
            uint32_t boneChannelCount, padding4;
            uint64_t morphTargetTableOffset;
            uint32_t morphTargetCount, padding5;
            uint64_t morphChannelTableOffset;
            uint32_t morphChannelCount, padding6;

            // [128..159] Динамические материалы
            uint64_t matPropertyTableOffset;
            uint32_t matPropertyCount, padding7;
            uint64_t matChannelTableOffset;
            uint32_t matChannelCount, padding8;

            // [160..191] Клипы и Граф сцены
            uint64_t clipTableOffset;
            uint32_t clipCount, padding9;
            uint64_t sceneGraphOffset;       // Плоский массив нод [т-ж]
            uint32_t sceneNodeCount, paddingBlob;
            uint64_t nodeLevelRangeTableOffset; // Смещение до массива NodeLevelRange[]
            uint32_t nodeLevelRangeCount;       // Максимальная глубина иерархии графа (кол-во уровней)
            uint32_t paddingLevels;             // Выравнивание до 16 байт

            // [192..239] Источники света (Раздельные массивы) [т-ж]
            uint64_t dirLightTableOffset;
            uint32_t dirLightCount, paddingLight0;
            uint64_t pointLightTableOffset;
            uint32_t pointLightCount, paddingLight1;
            uint64_t spotLightTableOffset;
            uint32_t spotLightCount, paddingLight2;

            // [240..255] Данные
            uint64_t bulkDataOffset;
            uint64_t bulkDataSize;
        };
        static_assert(sizeof(BlobHeader) == 272, "Size must be 272 bytes");

        // --- Метаданные материалов ---
        enum class MaterialPipelineFlagBits : uint32_t {
            eNone = 0,
            eAlphaTest = 1 << 0,   // Материал требует альфа-теста
            eTransparent = 1 << 1, // Материал прозрачный (alpha blending)
            eDoubleSided = 1 << 2, // Отрисовывать обе стороны полигона
            eUnlit = 1 << 3        // Материал не освещается (для UI, спецэффектов)
        };

        struct alignas(16) NodeLevelRange {
            uint32_t startNodeIdx;  // [0..3]   Индекс первой ноды этого уровня в плоском массиве
            uint32_t nodeCount;     // [4..7]   Количество нод на данном уровне глубины графа
            uint64_t padding;       // [8..15]  Выравнивание до 16 байт для единообразия
        };

        // Проверка компилятором, чтобы размер не «поплыл» из-за компиляторов
        static_assert(sizeof(NodeLevelRange) == 16, "Размер NodeLevelRange должен быть ровно 16 байт!");

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
            uint32_t padding;    // Заполнитель
        };

        // --- Метаданные текстур ---
        struct alignas(16) TextureMetaData {
            uint64_t textureOffset;
            uint64_t textureSize;

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
            int32_t  rootBoneIndex;    // Индекс корневой кости (INVALID_INDEX_I32 если нет)
            uint32_t explicitPadding;  // Явный паддинг, закрывающий 16-байтовую линию
        };
        static_assert(sizeof(SkeletonData) == 16, "Размер SkeletonData должен быть ровно 16 байт!");

        // =============================================================================
        // 2. ДАННЫЕ КОСТИ (Размер: ровно 80 байт)
        // =============================================================================
        struct alignas(16) BoneData {
            glm::mat4 invBindMatrix;      // [0..63]  Обратная матрица связывания для скиннинга (64 байта)
            int32_t   parentIndex;        // [64..67] Индекс родительской кости (INVALID_INDEX_I32 если нет, 4 байта)
            uint32_t  nodeIndex;          // [68..71] Индекс соответствующей ноды в графе (INVALID_INDEX_U32 если нет, 4 байта)
            uint32_t  explicitPadding[2]; // [72..79] Явный паддинг (8 байт). Добивает структуру до 80 байт (кратно 16)
        };
        static_assert(sizeof(BoneData) == 80, "Размер BoneData должен быть ровно 80 байт!");

        // =============================================================================
        // 3. КЛИП АНИМАЦИИ (Размер: ровно 32 байта)
        // =============================================================================
        // Имя клипа хранится как FNV-1a хэш; полное имя может быть восстановлено через String Pool во время загрузки
        struct alignas(16) AnimationClip {
            // [0..3] Длительность всей анимации в секундах
            float    duration;

            // [4..11] Скелетные треки костей
            uint32_t firstBoneChannelIdx;   // Индекс первого BoneChannel в глобальном буфере
            uint32_t boneChannelCount;      // Количество BoneChannel-ов в этом клипе

            // [12..19] Треки морфинга (мимики лица)
            uint32_t firstMorphChannelIdx;  // Индекс первого MorphChannel в глобальном буфере
            uint32_t morphChannelCount;     // Количество MorphChannel-ов в этом клипе

            // [20..23] Име клипа (хэш FNV-1a)
            uint32_t clipNameHash;          // Хэш имени клипа для идентификации

            // [24..27] Треки изменения параметров материалов
            uint32_t firstMatChannelIdx;    // Индекс первого MaterialChannel в глобальном буфере
            uint32_t matChannelCount;       // Количество MaterialChannel-ов в этом клипе

            // [28..31] Явный паддинг
            uint32_t explicitPadding;
        };
        // Жесткая проверка компилятора — размер действительно 48 байт
        static_assert(sizeof(AnimationClip) == 48, "Размер AnimationClip должен быть ровно 48 байт!");


        // =============================================================================
        // 4. КАНАЛ АНИМАЦИИ (Размер: ровно 32 байта)
        // =============================================================================
        struct alignas(16) AnimationChannel {
            uint32_t targetNodeIndex{};  // Индекс ноды/кости, которую анимируем
            uint32_t pathType{};         // 0: translation, 1: rotation (quat), 2: scale
            uint32_t samplerIndex{};     // Индекс AnimationSampler (на будущее)
            uint32_t keyframeOffset{};   // Смещение начала трека ОДНОВРЕМЕННО в буфере Times и буфере Values
            uint32_t keyframeCount{};    // Количество ключевых кадров в этом треке
            uint32_t explicitPadding[3]{0,0,0}; // Явный паддинг (12 байт). Закрывает структуру на отметке 32 байта
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

        struct alignas(16) MaterialChannel {
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
            glm::vec4 directionAndIntensity;        // [0..15]  Направление света (x, y, z) и интенсивность (w)

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
            float     innerCutOffCos;           // [44..47] cos(внутреннего угла)

            float     outerCutOffCos;           // [48..51] cos(внешнего угла)
            uint32_t  castShadows;           // [52..55] Флаг теней
            uint32_t  explicitPadding[2]{0,0};    // [56..63] Ручной паддинг. Структура ровно 64 байта.
        };
        static_assert(sizeof(SpotLight) == 64, "Размер SpotLight должен быть ровно 64 байта!");

        struct alignas(16) MorphTarget {
            uint32_t firstDeltaGlobalIdx; // [0..3]   Смещение первого MorphVertexDelta в глобальном буфере
            uint32_t deltaCount;          // [4..7]   Сколько ВСЕГО вершин деформирует эта цель
            uint32_t targetNameHash;      // [8..11]  Хэш имени цели (например, для связки с липсинк-системами)
            uint32_t explicitPadding;     // [12..15] Выравнивание первой 16-байтовой линии

            // Максимальный радиус и габариты деформации (нужны для точного Frustum Culling объекта)
            glm::vec3 maxPositionDelta;   // [16..27] На какую максимальную величину могут сдвинуться вершины
            uint32_t  explicitPadding2;   // [28..31] Добиваем структуру до 32 байт (std430 / alignas(16))
        };
        static_assert(sizeof(MorphTarget) == 32, "Размер MorphTarget должен быть ровно 32 байта!");

        struct alignas(16) MorphVertexDelta {
            glm::vec3 positionDelta;      // [0..11]  Вектор смещения позиции вершины (x, y, z)
            uint32_t  vertexIndex;        // [12..15] Оригинальный индекс вершины в базовом буфере меша,
            // которую нужно сдвинуть.
        };
        static_assert(sizeof(MorphVertexDelta) == 16, "Размер MorphVertexDelta должен быть ровно 16 байт!");

        struct alignas(16) MaterialProperty {
            uint32_t propertyHash;        // Хэш имени переменной в шейдере (например, "u_EmissionColor")
            uint32_t elementCount;        // Сколько компонентов (1 для float, 3 для vec3, 4 для vec4)
            uint32_t offsetInMaterial;    // Смещение параметра в байтах внутри структуры материала
            uint32_t explicitPadding;
        };
        static_assert(sizeof(MaterialProperty) == 16, "Размер MaterialProperty должен быть ровно 16 байт!");

        struct alignas(16) SceneNode {
            // [0..15] Локальная позиция и индекс родителя
            glm::vec3 localTranslation;    // [0..11]  Локальный сдвиг ноды
            uint32_t  parentIndex;         // [12..15] Индекс родительской ноды в плоском массиве (INVALID_INDEX_U32 = нет родителя)

            // [16..31] Локальное вращение (кватернион)
            glm::vec4 localRotationQuat;   // [16..31] Локальное вращение (x,y,z,w)

            // [32..63] Локальный масштаб, индекс меша и хэш имени
            glm::vec3 localScale;          // [32..43] Локальный масштаб
            uint32_t  instanceIndex;       // [44..47] Индекс первого меша ноды (INVALID_INDEX_U32 = нет меша, это чистая кость/помощник)

            uint32_t  nodeNameHash;        // [48..51] Хэш имени ноды (FNV-1a) для сопоставления с костями (INVALID_HASH = нет имени)
            uint32_t  explicitPadding;     // [52..55] Явный паддинг
            uint64_t  reservedPadding;     // [56..63] Зарезервировано на будущее
        };
        static_assert(sizeof(SceneNode) == 64, "Размер SceneNode должен быть ровно 64 байта!");
    } // namespace shuttle_engine::format