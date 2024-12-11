#pragma once
#include "OrderBook.h"
#include "Replay.h"
#include "MemoryPool.h"
#include "Logging.h"
#include "SymbolConfig.h"
#include <atomic>

class MatchingEngine {
public:
    MatchingEngine(const std::string& symbol, Replay& replay, MemoryPool<Order>& pool, SymbolConfigManager &configManager);

    double getLastTradePrice() const;
    bool processAdd(const AddMessage &msg);
    bool processCancel(const CancelMessage &msg);
    bool processCancelReplace(const CancelReplaceMessage &msg);
    void processSnapshotRequest(const SnapshotRequest &msg);
    void sendExecution(const ExecutionMessage &exec);

    void step();

    OrderBook orderBook;

private:
    std::string symbol_;
    Replay& replayLog;
    MemoryPool<Order>& orderPool;
    SymbolConfigManager &configManager;

    std::atomic<uint64_t> nextSequence{1};

    bool validateAdd(const AddMessage &msg);
    bool validateCancel(const CancelMessage &msg);
    bool validateCancelReplace(const CancelReplaceMessage &msg);

    bool checkVolatilityHalt(const AddMessage &msg);
    bool priceValidForSymbol(const std::string &symbol, double price);
    bool tickSizeValid(const std::string &symbol, double price);
    bool quantityValid(const std::string &symbol, uint64_t qty);
    bool checkTimeInForce(Order* o, std::vector<ExecutionMessage> &trades, uint64_t timestamp);

    void handleMarketOrder(Order* o, std::vector<ExecutionMessage> &trades, uint64_t timestamp);
    void handleIocFok(Order* o, std::vector<ExecutionMessage> &trades, uint64_t timestamp);
};

