#pragma once
#include <cstddef>
#include <memory>
#include <new>

namespace shuttle::async {

    // 1. БАЗОВЫЙ КЛАСС: Скрывает в себе прямые вызовы функций mimalloc
    class MiAllocatorBase {
    public:
        static void* operator new(std::size_t size) noexcept;
        static void operator delete(void* ptr) noexcept;

    protected:
        MiAllocatorBase() noexcept = default;
        ~MiAllocatorBase() noexcept = default;

        // Выделение сырой памяти с учетом аппаратного выравнивания
        [[nodiscard]] void* allocate_bytes(std::size_t bytes, std::size_t alignment) noexcept;

        // Освобождение сырой памяти
        void deallocate_bytes(void* ptr) noexcept;
    };

    // 2. ШАБЛОННЫЙ ФАСАД: Соответствует стандарту C++ Allocator Requirements
    template <typename T>
    class MiAllocator : MiAllocatorBase {
    public:
        using value_type = T;

        // Конструкторы обязаны быть constexpr по стандарту C++20
        constexpr MiAllocator() noexcept = default;

        // Шаблонный конструктор копирования для поддержки rebind (например, внутри std::list)
        template <typename U>
        constexpr MiAllocator(const MiAllocator<U>&) noexcept {}

        // Выделение памяти под массив из n элементов типа T
        [[nodiscard]] T* allocate(std::size_t n) noexcept {
            if (n == 0) [[unlikely]] return nullptr;

            // Вычисляем размер и запрашиваем выровненные байты у базового класса
            void* p = this->allocate_bytes(n * sizeof(T), alignof(T));
            return static_cast<T*>(p);
        }

        // Освобождение памяти
        void deallocate(T* p, std::size_t size = 0) noexcept {
            if (p == nullptr) [[unlikely]] return;
            this->deallocate_bytes(p);
        }

    };

    // Операторы сравнения обязательны по стандарту.
    // Наш аллокатор stateless (без полей состояния), поэтому любые два экземпляра всегда равны,
    // а значит, память, выделенная одним, может быть безопасно удалена другим.
    template <typename T, typename U>
    [[nodiscard]] constexpr bool operator==(const MiAllocator<T>&, const MiAllocator<U>&) noexcept {
        return true;
    }

    template <typename T, typename U>
    [[nodiscard]] constexpr bool operator!=(const MiAllocator<T>&, const MiAllocator<U>&) noexcept {
        return false;
    }

    template <typename T>
    struct MiDeleter {
        // Делитель stateless, компилятор полностью сотрет его в unique_ptr (EBO optimization)
        void operator()(T* ptr) const noexcept {
            if (ptr == nullptr) [[unlikely]] return;

            // 1. Явно вызываем деструктор объекта типа T
            ptr->~T();

            // 2. Освобождаем память через наш MiAllocator
            MiAllocator<T> alloc;
            alloc.deallocate(ptr, 1);
        }
    };

    // Удобный, бинарно-безопасный псевдоним умного указателя движка Shuttle Engine
    template <typename T>
    using MiUniquePtr = std::unique_ptr<T, MiDeleter<T>>;

    template <typename T, typename... Args>
    [[nodiscard]] MiUniquePtr<T> mi_make_unique_nothrow(Args&&... args) noexcept {
        // ЖЕСТКИЙ СТАТИЧЕСКИЙ ПРЕДОХРАНИТЕЛЬ:
        // Запрещаем сборку, если конструктор типа T с заданными аргументами может бросить исключение!
        static_assert(std::is_nothrow_constructible_v<T, Args...>,
            "Critical Error: Type T must be completely noexcept constructible with provided arguments!");

        MiAllocator<T> alloc;

        // 1. Выделяем сырую память под один объект типа T через мималлок
        T* memory = alloc.allocate(1);

        // Предохранитель на случай тотальной нехватки RAM на хосте
        if (!memory) [[unlikely]] {
            return MiUniquePtr<T>(nullptr);
        }

        // 2. Конструируем объект прямо по вычисленному адресу (Placement New).
        // Благодаря static_assert выше, мы на 100% уверены, что эта строчка безопасна.
        T* ptr = ::new (static_cast<void*>(memory)) T(std::forward<Args>(args)...);

        // 3. Упаковываем в наш кастомный умный указатель и возвращаем по семантике перемещения
        return MiUniquePtr<T>(ptr);
    }
} // namespace shuttle::async
