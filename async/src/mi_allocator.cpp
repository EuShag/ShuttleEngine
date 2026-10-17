#include <async/mi_allocator.hpp>
#include <mimalloc.h>
#include <mimalloc-new-delete.h>

namespace shuttle::async {
    void* MiAllocatorBase::operator new(std::size_t size) noexcept {
        return mi_malloc(size);
    }

    void MiAllocatorBase::operator delete(void* ptr) noexcept {
        mi_free(ptr);
    }

    void* MiAllocatorBase::allocate_bytes(std::size_t bytes, std::size_t alignment) noexcept {
        // Используем mi_malloc_aligned, чтобы гарантировать, что тяжелые графические
        // структуры (например, матрицы трансформаций) лягут строго по границам кэш-линий CPU
        void* ptr = mi_malloc_aligned(bytes, alignment);

        // Так как исключения отключены, при нехватке памяти (OOM)
        // мы детерминированно возвращаем nullptr вместо throw std::bad_alloc
        return ptr;
    }

    void MiAllocatorBase::deallocate_bytes(void* ptr) noexcept {
        // mimalloc сам атомарно определит, какому потоку и странице принадлежит этот адрес
        mi_free(ptr);
    }

} // namespace shuttle::async
