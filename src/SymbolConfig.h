#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

struct SymbolConfig {
    double tickSize;
    uint64_t minQuantity;
    double minPrice;
    double maxPrice;
    double volatilityThreshold; // e.g. max percent change from reference
    double referencePrice;      // base price for volatility checks
    bool tradingHalted;
};

class SymbolConfigManager {
public:
    void setConfig(const std::string &symbol, const SymbolConfig &config) {
        std::lock_guard<std::mutex> lock(mtx_);
        configs_[symbol] = config;
    }

    bool getConfig(const std::string &symbol, SymbolConfig &out) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = configs_.find(symbol);
        if (it == configs_.end()) return false;
        out = it->second;
        return true;
    }

    void haltTrading(const std::string &symbol) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (configs_.find(symbol) != configs_.end()) {
            configs_[symbol].tradingHalted = true;
        }
    }

    void resumeTrading(const std::string &symbol) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (configs_.find(symbol) != configs_.end()) {
            configs_[symbol].tradingHalted = false;
        }
    }

private:
    std::unordered_map<std::string, SymbolConfig> configs_;
    std::mutex mtx_;
};

