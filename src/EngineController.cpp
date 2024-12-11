#include "EngineController.h"
#include "Logging.h"

EngineController::EngineController(Replay &replay, SymbolConfigManager &cfg)
    : replayLog(replay), configManager(cfg) {}

void EngineController::addEngineForSymbol(const std::string &symbol, double tickSize, uint64_t minQty, double minP, double maxP, double volThreshold, double refPrice) {
    std::unique_lock lock(enginesMutex);
    if (engines.find(symbol) != engines.end()) {
        LOG(LogLevel::WARN, "addEngineForSymbol: already have engine for symbol");
        return;
    }

    SymbolConfig sc;
    sc.tickSize = tickSize;
    sc.minQuantity = minQty;
    sc.minPrice = minP;
    sc.maxPrice = maxP;
    sc.volatilityThreshold = volThreshold;
    sc.referencePrice = refPrice;
    sc.tradingHalted = false;
    configManager.setConfig(symbol, sc);

    MatchingEngine* engine = new MatchingEngine(symbol, replayLog, orderPool, configManager);
    engines[symbol] = engine;
}

bool EngineController::dispatchAdd(const AddMessage &msg) {
    std::shared_lock lock(enginesMutex);
    auto it = engines.find(msg.symbol);
    if (it == engines.end()) {
        LOG(LogLevel::ERROR, "dispatchAdd: No engine for symbol");
        return false;
    }
    bool success = it->second->processAdd(msg);
    if (success) {
        std::lock_guard<std::mutex> l(orderSymbolMapMutex);
        orderSymbolMap[msg.orderId] = msg.symbol;
    }
    return success;
}

bool EngineController::dispatchCancel(const CancelMessage &msg) {
    std::string sym;
    if (!findOrderSymbol(msg.orderId, sym)) {
        LOG(LogLevel::ERROR, "dispatchCancel: Unknown orderId");
        return false;
    }

    std::shared_lock lock(enginesMutex);
    auto it = engines.find(sym);
    if (it == engines.end()) {
        LOG(LogLevel::ERROR, "dispatchCancel: No engine for symbol");
        return false;
    }
    return it->second->processCancel(msg);
}

bool EngineController::dispatchCancelReplace(const CancelReplaceMessage &msg) {
    std::string sym;
    if (!findOrderSymbol(msg.orderId, sym)) {
        LOG(LogLevel::ERROR, "dispatchCancelReplace: Unknown orderId");
        return false;
    }

    std::shared_lock lock(enginesMutex);
    auto it = engines.find(sym);
    if (it == engines.end()) {
        LOG(LogLevel::ERROR, "dispatchCancelReplace: No engine for symbol");
        return false;
    }
    return it->second->processCancelReplace(msg);
}

void EngineController::dispatchSnapshotRequest(const SnapshotRequest &msg) {
    std::shared_lock lock(enginesMutex);
    auto it = engines.find(msg.symbol);
    if (it == engines.end()) {
        LOG(LogLevel::ERROR, "dispatchSnapshotRequest: No engine for symbol");
        return;
    }
    it->second->processSnapshotRequest(msg);
}

double EngineController::getLastTradePrice(const std::string &symbol) const {
    std::shared_lock lock(enginesMutex);
    auto it = engines.find(symbol);
    if (it == engines.end()) {
        throw std::runtime_error("getLastTradePrice: No machine engine for symbol");
    }
    return it->second->getLastTradePrice();
}

void EngineController::getTopOfBook(const std::string &symbol, double &bestBid, double &bestAsk) {
    std::shared_lock lock(enginesMutex);
    auto it = engines.find(symbol);
    if (it == engines.end()) {
        throw std::runtime_error("getTopOfBook: No engine for symbol " + symbol);
    }
    it->second->orderBook.getTopOfBook(bestBid, bestAsk);
}

void EngineController::recordOrderSymbol(uint64_t orderId, const std::string &symbol) {
    std::lock_guard<std::mutex> lock(orderSymbolMapMutex);
    orderSymbolMap[orderId] = symbol;
}

bool EngineController::findOrderSymbol(uint64_t orderId, std::string &symbol) {
    std::lock_guard<std::mutex> lock(orderSymbolMapMutex);
    auto it = orderSymbolMap.find(orderId);
    if (it == orderSymbolMap.end()) return false;
    symbol = it->second;
    return true;
}

