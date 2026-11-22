#pragma once
#include <new>
#include <cassert>
#include <cstddef>
#include <cstdint>
template<typename T>
class PoolAllocator{

    char* pool_;
    std::size_t PoolSize;

    struct FreeNode{
        FreeNode* next;
    };

    FreeNode* free_head = nullptr;

    void initialize_pool(){
        pool_ = static_cast<char*>(:: operator new(PoolSize * sizeof(T)));
        char* current = pool_;
        for(std::size_t i = 0; i < PoolSize - 1; ++i){
            reinterpret_cast<FreeNode*>(current)->next = reinterpret_cast<FreeNode*>(current + sizeof(T));
            current += sizeof(T);
        }

        reinterpret_cast<FreeNode*>(current)->next = nullptr;

        free_head = reinterpret_cast<FreeNode*>(pool_);
    }

    public:

    explicit PoolAllocator(std::size_t size):PoolSize(size){
        assert(size > 0 && "Pool size must be greater than zero");
        initialize_pool();
    }

    ~PoolAllocator() {
        if (pool_ != nullptr) {
            ::operator delete(pool_);
        }
    }

    // FIXED: Prevent accidental pool duplication and memory corruption
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    T* allocate(std::size_t n = 1) {
        // In debug builds, this catches bugs. 
        // In release builds (compiled with -DNDEBUG), these compile away to zero instructions.
        assert(n == 1 && "Pool only handles single objects");
        assert(free_head != nullptr && "Memory pool exhausted");

        T* result = reinterpret_cast<T*>(free_head);
        free_head = free_head->next;

        return result;
    }

    // void deallocate(T* p, std::size_t n = 1) noexcept {
    //     if (n != 1 || p == nullptr) return;

    //          // Verify pointer is within our pool
    //     if (p < reinterpret_cast<T*>(pool_) || 
    //         p >= reinterpret_cast<T*>(pool_ + (PoolSize * sizeof(T)))) {
    //         return; // Not our memory, ignore
    //     }

    //     FreeNode* node = reinterpret_cast<FreeNode*>(p);
    //     node->next = free_head;
    //     free_head = node;
    // }

    void deallocate(T* p, std::size_t n = 1) noexcept {
    if (n != 1 || p == nullptr) return;

    // Cast to std::uintptr_t for safe, raw memory address comparison
    auto p_addr = reinterpret_cast<std::uintptr_t>(p);
    auto pool_start = reinterpret_cast<std::uintptr_t>(pool_);
    auto pool_end = pool_start + (PoolSize * sizeof(T));

    if (p_addr < pool_start || p_addr >= pool_end) {
        return; // Not our memory, ignore
    }

    FreeNode* node = reinterpret_cast<FreeNode*>(p);
    node->next = free_head;
    free_head = node;
}
    
};