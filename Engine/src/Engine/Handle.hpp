//
// Created by Shagu on 07.09.2026.
//

#ifndef SHUTTLEENGINE_HANDLE_HPP
#define SHUTTLEENGINE_HANDLE_HPP
#include <cinttypes>
#include <memory>

namespace shuttle::engine {

    // 1. Специальный теговый тип для нулевого хэндла (аналог std::nullptr_t)
    struct NullHandle_t {
        struct SecretTag {};
        constexpr explicit NullHandle_t(SecretTag) noexcept {}
    };

    // 2. Глобальная константа нулевого хэндла (аналог nullptr)
    inline constexpr NullHandle_t NullHandle{NullHandle_t::SecretTag{}};

    // Базовый Handle
    struct Handle {
        uint64_t id = 0;

        constexpr Handle() noexcept = default;
        constexpr explicit Handle(uint64_t rawId) noexcept : id(rawId) {}
        constexpr Handle(uint32_t index, uint32_t generation) noexcept
            : id((static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index)) {}

        // Разрешаем неявное конструирование из NullHandle!
        constexpr Handle(NullHandle_t) noexcept : id(0) {}

        [[nodiscard]] constexpr uint32_t index() const noexcept { return static_cast<uint32_t>(id & 0xFFFFFFFF); }
        [[nodiscard]] constexpr uint32_t generation() const noexcept { return static_cast<uint32_t>(id >> 32); }
        [[nodiscard]] constexpr bool isValid() const noexcept { return id != 0; }

        explicit operator bool() const noexcept { return isValid(); }
        auto operator<=>(const Handle&) const = default;
        constexpr bool operator==(const Handle& other) const noexcept { return id == other.id; }
    };

    // Строго типизированный хэндл
    template<typename>
    class TypedHandle {
    public:
        TypedHandle() = default;
        constexpr explicit TypedHandle(Handle other) noexcept : id{other} {}
        constexpr TypedHandle(uint32_t index, uint32_t generation) noexcept : id{index, generation} {}

        // НЕЯВНЫЙ конструктор и оператор присваивания из NullHandle!
        constexpr TypedHandle(NullHandle_t) noexcept {}

        TypedHandle& operator=(NullHandle_t) noexcept {
            id = Handle{0};
            return *this;
        }

        [[nodiscard]] constexpr operator Handle() const noexcept { return id; }
        [[nodiscard]] constexpr Handle get() const noexcept { return id; }
        [[nodiscard]] constexpr uint32_t index() const noexcept { return id.index(); }
        [[nodiscard]] constexpr uint32_t generation() const noexcept { return id.generation(); }
        [[nodiscard]] constexpr bool isValid() const noexcept { return id.isValid(); }

        explicit operator bool() const noexcept { return isValid(); }

        auto operator<=>(const TypedHandle&) const = default;

        // Операторы сравнения с NullHandle
        friend constexpr bool operator==(const TypedHandle& h, NullHandle_t) noexcept { return !h.isValid(); }
        friend constexpr bool operator!=(const TypedHandle& h, NullHandle_t) noexcept { return h.isValid(); }
        friend constexpr bool operator==(NullHandle_t, const TypedHandle& h) noexcept { return !h.isValid(); }
        friend constexpr bool operator!=(NullHandle_t, const TypedHandle& h) noexcept { return h.isValid(); }

        constexpr bool operator==(const TypedHandle& other) const noexcept {
            return id == other.id;
        }

    private:
        Handle id{0};
    };

    inline constexpr Handle NULL_HANDLE = Handle{};

    template<typename Handle, typename Deleter>
    requires std::is_invocable_v<Deleter, Handle>
    struct UniqueHandle {
        Handle handle;
        Deleter deleter;

        UniqueHandle() : handle{}, deleter{} {}
        UniqueHandle(Handle h, Deleter d) : handle(h), deleter(d) {}

        UniqueHandle(UniqueHandle const&) = delete;
        UniqueHandle& operator=(UniqueHandle const&) = delete;

        UniqueHandle(UniqueHandle&& other) noexcept
            : handle(other.handle), deleter(std::move(other.deleter)) {
            other.handle = Handle{};
        }

        Handle operator*() const noexcept {
            return handle;
        }

        Handle get() const noexcept {
            return handle;
        }

        UniqueHandle& operator=(UniqueHandle&& other) noexcept {
            if (this != &other) {
                deleter(handle);
                handle = other.handle;
                deleter = std::move(other.deleter);
                other.handle = Handle{};
            }
            return *this;
        }

        void reset() {
            if (handle != Handle{}) {
                deleter(handle);
            }
        }

        ~UniqueHandle() {
            if (handle != Handle{}) {
                deleter(handle);
            }
        }
    };
}

#endif //SHUTTLEENGINE_HANDLE_HPP
