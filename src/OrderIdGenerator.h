#pragma once
#include <atomic>

class OrderIdGenerator {
public:
    static uint64_t nextId() {
        return idCounter.fetch_add(1, std::memory_order_relaxed);
    }
private:
    static std::atomic<uint64_t> idCounter;
};

