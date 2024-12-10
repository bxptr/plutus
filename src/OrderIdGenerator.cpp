#include "OrderIdGenerator.h"

std::atomic<uint64_t> OrderIdGenerator::idCounter{1};

