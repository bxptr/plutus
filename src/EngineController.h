#pragma once
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include "MatchingEngine.h"
#include "Replay.h"
#include "MemoryPool.h"
#include "SymbolConfig.h"

class EngineController {
public:
    EngineController(Replay &replay, SymbolConfigManager &configManager);

    bool dispatchAdd(const AddMessage &msg);
    bool dispatchCancel(const CancelMessage &msg);
    bool dispatchCancelReplace(const CancelReplaceMessage &msg);
    void dispatchSnapshotRequest(const SnapshotRequest &msg);
    double getLastTradePrice(const std::string &symbol) const;
    
    void addEngineForSymbol(const std::string &symbol, double tickSize, uint64_t minQty, double minP, double maxP, double volThreshold, double refPrice);

private:
    std::unordered_map<std::string, MatchingEngine*> engines;
    mutable std::shared_mutex enginesMutex; 
    Replay &replayLog;
    MemoryPool<Order> orderPool; 
    SymbolConfigManager &configManager;

    // We'll need a map orderId->symbol for cancel
    std::unordered_map<uint64_t, std::string> orderSymbolMap;
    std::mutex orderSymbolMapMutex;

    void recordOrderSymbol(uint64_t orderId, const std::string &symbol);
    bool findOrderSymbol(uint64_t orderId, std::string &symbol);
};

