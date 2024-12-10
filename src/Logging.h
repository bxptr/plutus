#pragma once
#include <iostream>
#include <mutex>
#include <string_view>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

inline LogLevel GLOBAL_LOG_LEVEL = LogLevel::INFO;
inline std::mutex LOG_MUTEX;

#define LOG(level, msg) \
    do { \
        if (level >= GLOBAL_LOG_LEVEL) { \
            std::lock_guard<std::mutex> lock(LOG_MUTEX); \
            std::cerr << #level << ": " << msg << "\n"; \
        } \
    } while(0)

