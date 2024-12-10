#pragma once
#include <cstddef>
#include <vector>
#include <mutex>

template<typename T, size_t BLOCK_SIZE=1024>
class MemoryPool {
public:
    MemoryPool() {
        allocateBlock();
    }

    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (freeList_.empty()) {
            allocateBlock();
        }
        T* obj = freeList_.back();
        freeList_.pop_back();
        return obj;
    }

    void deallocate(T* obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        freeList_.push_back(obj);
    }

private:
    void allocateBlock() {
        blockStorage_.emplace_back(new char[sizeof(T) * BLOCK_SIZE]);
        char* block = blockStorage_.back();
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            freeList_.push_back(reinterpret_cast<T*>(block + i * sizeof(T)));
        }
    }

    std::vector<char*> blockStorage_;
    std::vector<T*> freeList_;
    std::mutex mutex_;
};

