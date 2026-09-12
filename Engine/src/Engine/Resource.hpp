//
// Created by Shagu on 07.09.2026.
//

#ifndef SHUTTLEENGINE_RESOURCE_HPP
#define SHUTTLEENGINE_RESOURCE_HPP
#include <atomic>
#include <optional>
#include <vector>

#include "concurrentqueue.h"

namespace shuttle::engine {

    constexpr auto INITIAL_GENERATION = 1U;

    template<typename TResource>
    class Resource {
    public:
        Resource() = default;
        explicit Resource(TResource&& res) : resource{std::move(res)} {}

        Resource(Resource&&) = delete;
        Resource(Resource const&) = delete;
        Resource& operator=(Resource const&) = delete;
        Resource& operator=(Resource&&) = delete;

        // Замена ресурса новым по месту (In-Place Construction)
        void replace(TResource&& newResource) {
            // 1. Старый ресурс автоматически уничтожается (~TResource)
            // 2. На его МЕСТО в памяти конструируется новый newResource через Placement New!
            resource.emplace(std::move(newResource));
        }

        // Метод проверки: совпадает ли поколение хэндла с поколением слота?
        [[nodiscard]] bool validateGeneration(uint32_t handleGen) const noexcept {
            return hasValue() && (generation.load(std::memory_order_relaxed) == handleGen);
        }

        [[nodiscard]] uint32_t getGeneration() const noexcept {
            return generation.load(std::memory_order_relaxed);
        }

        // Запрос на удаление (вызывается из Главного Потока)
        [[nodiscard]] bool setDestroy() {
            mustDestroy.store(true, std::memory_order_release);
            return tryDestroy();
        }

        // Захват ресурса на время работы GPU
        void acquire() noexcept {
            inFlightCounter.fetch_add(1, std::memory_order_relaxed);
        }

        // Освобождение из FenceWaiter (Фоновый Поток)
        [[nodiscard]] bool release() {
            inFlightCounter.fetch_sub(1, std::memory_order_release); // release гарантирует видимость завершения работ
            return tryDestroy();
        }

        // Быстрая проверка: помечен ли ресурс на удаление?
        [[nodiscard]] bool isPendingDelete() const noexcept {
            return mustDestroy.load(std::memory_order_relaxed);
        }

        bool tryDestroy() {
            // Читаем счетчик и флаг уничтожения
            if (inFlightCounter.load(std::memory_order_acquire) == 0 &&
                mustDestroy.load(std::memory_order_acquire))
            {
                // Атомарно сбрасываем флаг, чтобы только ОДИН поток выполнял уничтожение!
                bool expected = true;
                if (mustDestroy.compare_exchange_strong(expected, false)) {
                    if (resource.has_value()) {
                        // 1. Уничтожаем ресурс (освобождаем VRAM)
                        TResource destroyedResource = std::move(*resource);
                        resource.reset();

                        // 2. ИНКРЕМЕНТИРУЕМ ПОКОЛЕНИЕ СЛОТА!
                        // Все старые хэндлы со старым поколением теперь инвалидированы!
                        generation.fetch_add(1, std::memory_order_release);
                        // 3. Если ресурс уничтожен, возвращаем true
                        return true;
                    }
                }
                return false;
            }
            return false;
        }

        [[nodiscard]] TResource* get() noexcept {
            return resource.has_value() ? &(*resource) : nullptr;
        }

        [[nodiscard]] bool hasValue() const noexcept {
            return resource.has_value();
        }

    private:
        std::optional<TResource> resource;       // std::optional позволяет делать .reset()
        std::atomic<uint32_t> generation{INITIAL_GENERATION};
        std::atomic<uint64_t> inFlightCounter{0}; // Атомарный счетчик полета GPU
        std::atomic<bool> mustDestroy{false};     // АТОМАРНЫЙ флаг удаления!
    };

    template<typename TResource>
    struct ResourceTraits;

    template <typename T, size_t BlockSize = 512>
    class SegmentedVector {
        static_assert((BlockSize & (BlockSize - 1)) == 0, "BlockSize must be a power of 2!");

        struct alignas(alignof(T)) RawStorage {
            std::byte data[sizeof(T)];
            T* ptr() noexcept { return reinterpret_cast<T*>(data); }
            const T* ptr() const noexcept { return reinterpret_cast<const T*>(data); }
        };

        // Фиксированный максимум блоков. 65536 блоков по 512 элементов = ~33.5 млн элементов.
        // Память под указатели (65536 * 8 байт = 512 КБ) выделяется сразу, но сами блоки — лениво.
        static constexpr size_t MaxBlocks = 65536;

    public:
        SegmentedVector() {
            // Инициализируем атомарные указатели в null
            for (auto& block : m_blocks) {
                block.store(nullptr, std::memory_order_relaxed);
            }
        }

        ~SegmentedVector() {
            size_t current_size = m_size.load(std::memory_order_relaxed);
            // Уничтожаем только реально созданные объекты
            for (size_t i = 0; i < current_size; ++i) {
                std::destroy_at(&operator[](i));
            }
            // Освобождаем выделенные блоки сырой памяти
            for (auto& block : m_blocks) {
                delete[] block.load(std::memory_order_relaxed);
            }
        }

        // Запрет копирования и перемещения в многопоточной среде
        SegmentedVector(const SegmentedVector&) = delete;
        SegmentedVector& operator=(const SegmentedVector&) = delete;
        SegmentedVector(SegmentedVector&&) = delete;
        SegmentedVector& operator=(SegmentedVector&&) = delete;

        // Быстрый lock-free доступ O(1)
        T& operator[](size_t index) noexcept {
            size_t blockIdx = index / BlockSize;
            size_t elementIdx = index % BlockSize;

            // memory_order_acquire гарантирует, что мы увидим сконструированный объект
            RawStorage* block = m_blocks[blockIdx].load(std::memory_order_acquire);
            return *(block[elementIdx].ptr());
        }

        const T& operator[](size_t index) const noexcept {
            size_t blockIdx = index / BlockSize;
            size_t elementIdx = index % BlockSize;

            const RawStorage* block = m_blocks[blockIdx].load(std::memory_order_acquire);
            return *(block[elementIdx].ptr());
        }

        template <typename... Args>
        T& emplace_back(Args&&... args) {
            // 1. Бронируем индекс атомарно
            size_t index = m_size.fetch_add(1, std::memory_order_relaxed);

            size_t blockIdx = index / BlockSize;
            size_t elementIdx = index % BlockSize;

            // 2. Проверяем/выделяем блок (Lock-free / CAS-loop)
            RawStorage* block = m_blocks[blockIdx].load(std::memory_order_acquire);
            if (!block) {
                auto* newBlock = new RawStorage[BlockSize];
                RawStorage* expected = nullptr;

                // Пытаемся атомарно записать наш блок.
                // Если другой поток успел раньше, удаляем свой локальный и берем блок конкурента.
                if (!m_blocks[blockIdx].compare_exchange_strong(expected, newBlock,
                                                                std::memory_order_release,
                                                                std::memory_order_acquire)) {
                    delete[] newBlock;
                    block = expected;
                } else {
                    block = newBlock;
                }
            }

            // 3. Конструируем объект
            T* slotPtr = block[elementIdx].ptr();
            std::construct_at(slotPtr, std::forward<Args>(args)...);

            return *slotPtr;
        }

        [[nodiscard]] size_t size() const noexcept {
            return m_size.load(std::memory_order_relaxed);
        }

    private:
        std::array<std::atomic<RawStorage*>, MaxBlocks> m_blocks;
        std::atomic<size_t> m_size{0};
    };

    template <typename TResource>
    class SlotMapStorage {
        // Псевдоним для удобства, чтобы не писать длинную колбасу в методах
        using HandleType = ResourceTraits<TResource>::handleType;
        // Размер пачки индексов при первичном создании или исчерпании пула
        static constexpr uint32_t ALLOC_BATCH_SIZE = 512;
    public:
        SlotMapStorage() {
            // Опционально: можем сразу предвыделить первый батч индексов на старте!
            addNewBatchToPool();
        }

        template <typename... Args>
        HandleType allocate(Args&&... args) {
            uint32_t index = 0;

            // Пытаемся забрать индекс из очереди.
            // Если очередь пуста (все 512 штук разобрали) — мгновенно подмешиваем новый батч!
            if (!m_freeIndices.try_dequeue(index)) {
                addNewBatchToPool();

                // Теперь повторный dequeue гарантированно успешен
                bool success = m_freeIndices.try_dequeue(index);
                assert(success && "Failed to dequeue index even after expanding pool!");
            }

            // Переиспользуем слот по найденному индексу "по месту"
            m_slots[index].replace(std::forward<Args>(args)...);

            uint32_t currentGen = m_slots[index].getGeneration();
            return HandleType{index, currentGen};
        }


        // 2. БЫСТРЫЙ ДОСТУП по Хэндлу (Защита от ABA и устаревших хэндлов!)
        [[nodiscard]] TResource* get(HandleType handle) noexcept {
            uint32_t idx = handle.index();

            if (idx >= m_slots.size()) return nullptr;

            auto& slot = m_slots[idx];

            // МГНОВЕННАЯ ИНВАЛИДАЦИЯ:
            // 1. Проверяем поколение (не устарел ли хэндл из прошлого цикла)
            // 2. Проверяем, не помечен ли слот на удаление (setDestroy)!
            if (slot.getGeneration() != handle.generation() || slot.isPendingDelete()) {
                return nullptr; // ХЭНДЛ НЕВАЛИДЕН ПРЯМО СЕЙЧАС!
            }

            return slot.get(); // Возвращаем ресурс только если он жив и активен
        }


        // 3. ЗАПРОС НА УДАЛЕНИЕ (Пометка слота)
        void markForDeletion(HandleType handle) {
            uint32_t idx = handle.index();
            if (idx < m_slots.size()) {
                auto& slot = m_slots[idx];
                if (slot.getGeneration() == handle.generation()) {
                    slot.setDestroy(); // Помечаем, что ресурс должен удалиться
                }
            }
        }

        // 4. ФОНОВАЯ ОЧИСТКА (Вызывается из FenceWaiter или в конце кадра)
        void processCleanup() {
            size_t currentSize = m_slots.size();
            for (uint32_t i = 0; i < currentSize; ++i) {
                if (m_slots[i].release()) {
                    // Когда ресурс удален, возвращаем его индекс обратно в общий пул!
                    m_freeIndices.enqueue(i);
                }
            }
        }

    private:
        void addNewBatchToPool() {
            // Защищаем только само расширение массива блоков через мьютекс (или наш атомарный счётчик)
            std::lock_guard lock(m_growthMutex);

            auto startIndex = static_cast<uint32_t>(m_slots.size()); // Или текущий размер
            // Выделяем пачку слотов в SegmentedVector
            for (uint32_t i = 0; i < ALLOC_BATCH_SIZE; ++i) {
                uint32_t newIndex = startIndex + i;

                // Создаем пустой слот в памяти (без реального ресурса внутри, просто пустая обертка)
                // Убедись, что у Resource есть конструктор по умолчанию (Resource() = default)
                m_slots.emplace_back();

                // Закидываем этот индекс в Lock-Free очередь свободных
                m_freeIndices.enqueue(newIndex);
            }
        }

        SegmentedVector<Resource<TResource>> m_slots;
        moodycamel::ConcurrentQueue<uint32_t> m_freeIndices;
        std::mutex m_growthMutex; // Срабатывает ОЧЕНЬ редко (раз на 512 аллокаций!)
    };
}

#endif //SHUTTLEENGINE_RESOURCE_HPP
