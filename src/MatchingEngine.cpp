#include "MatchingEngine.h"
#include <chrono>
#include <cmath>

MatchingEngine::MatchingEngine(const std::string& sym, Replay& replay, MemoryPool<Order>& pool, SymbolConfigManager &cfg)
    : symbol_(sym), replayLog(replay), orderPool(pool), configManager(cfg) {
    orderBook.setMemoryPool(&orderPool);
}

bool MatchingEngine::validateAdd(const AddMessage &msg) {
    if (msg.symbol.size() > 7 || msg.quantity == 0) {
        LOG(LogLevel::ERROR, "Invalid AddMessage basic checks");
        return false;
    }

    if (!quantityValid(msg.symbol, msg.quantity)) {
        LOG(LogLevel::WARN, "validateAdd: Quantity below min");
        return false;
    }

    if (msg.orderType == OrderType::LIMIT || msg.orderType == OrderType::ICEBERG) {
        if (msg.price <= 0) {
            LOG(LogLevel::ERROR, "Invalid AddMessage price <=0 for limit/iceberg");
            return false;
        }
        if (!tickSizeValid(msg.symbol, msg.price)) {
            LOG(LogLevel::WARN, "validateAdd: price not aligned to tickSize");
            return false;
        }
        if (!priceValidForSymbol(msg.symbol, msg.price)) {
            LOG(LogLevel::WARN, "validateAdd: price out of allowed range");
            return false;
        }
    }

    if (msg.orderType == OrderType::STOP_LOSS) {
        if (msg.triggerPrice <= 0) {
            LOG(LogLevel::ERROR, "Stop order invalid triggerPrice");
            return false;
        }
    }

    if (checkVolatilityHalt(msg)) {
        LOG(LogLevel::WARN, "Trading halted for symbol due to volatility");
        return false;
    }

    return true;
}

bool MatchingEngine::validateCancel(const CancelMessage &msg) {
    if (msg.orderId == 0) {
        LOG(LogLevel::ERROR, "Invalid CancelMessage orderId=0");
        return false;
    }
    return true;
}

bool MatchingEngine::validateCancelReplace(const CancelReplaceMessage &msg) {
    if (msg.orderId == 0 || msg.newPrice <= 0 || msg.newQuantity == 0) {
        LOG(LogLevel::ERROR, "Invalid CancelReplaceMessage");
        return false;
    }
    if (!tickSizeValid(symbol_, msg.newPrice)) {
        LOG(LogLevel::WARN, "cancelReplace: new price not aligned to tickSize");
        return false;
    }
    if (!quantityValid(symbol_, msg.newQuantity)) {
        LOG(LogLevel::WARN, "cancelReplace: quantity below min");
        return false;
    }
    if (!priceValidForSymbol(symbol_, msg.newPrice)) {
        LOG(LogLevel::WARN, "cancelReplace: price out of allowed range");
        return false;
    }
    return true;
}

double MatchingEngine::getLastTradePrice() const {
    return orderBook.getLastTradePrice();
}

bool MatchingEngine::processAdd(const AddMessage &msg) {
    if (!validateAdd(msg)) return false;

    SymbolConfig cfg;
    if (!configManager.getConfig(msg.symbol, cfg)) {
        LOG(LogLevel::ERROR, "No config for symbol");
        return false;
    }
    if (cfg.tradingHalted) {
        LOG(LogLevel::WARN, "Trading halted for symbol");
        return false;
    }

    auto timestamp = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
    Order* o = orderPool.allocate();
    new(o) Order(msg.orderId, msg.side, msg.symbol, msg.price, msg.quantity, timestamp,
                 msg.participantId, msg.tif, msg.orderType, msg.triggerPrice, msg.visibleQuantity);

    // Write-ahead log
    replayLog.logAddMessage(msg.header.sequence, msg);

    std::vector<ExecutionMessage> trades;

    if (o->orderType == OrderType::MARKET) {
        // Market orders try to match immediately
        handleMarketOrder(o, trades, timestamp);
    } else if (o->tif != TimeInForce::GTC) {
        // IOC/FOK logic
        handleIocFok(o, trades, timestamp);
    } else {
        // GTC limit/iceberg/stop
        std::unique_lock<std::shared_mutex> lock(orderBook.bookMutex);
        if (!orderBook.addOrder(o)) {
            orderPool.deallocate(o);
            return false;
        }
        // If limit order just placed, try match
        if (o->orderType == OrderType::LIMIT || o->orderType == OrderType::ICEBERG) {
            auto res = orderBook.matchBook(nextSequence.load(), timestamp);
            for (auto &t : res) {
                nextSequence.store(t.header.sequence + 1);
                sendExecution(t);
            }
        }
    }

    // Send executions
    for (auto &t : trades) {
        nextSequence.store(t.header.sequence + 1);
        sendExecution(t);
    }

    return true;
}

bool MatchingEngine::processCancel(const CancelMessage &msg) {
    if (!validateCancel(msg)) return false;
    replayLog.logCancelMessage(msg.header.sequence, msg);

    std::unique_lock<std::shared_mutex> lock(orderBook.bookMutex);
    bool success = orderBook.cancelOrder(msg.orderId, msg.participantId);
    return success;
}

bool MatchingEngine::processCancelReplace(const CancelReplaceMessage &msg) {
    if (!validateCancelReplace(msg)) return false;

    replayLog.logCancelReplaceMessage(msg.header.sequence, msg);

    std::unique_lock<std::shared_mutex> lock(orderBook.bookMutex);
    bool success = orderBook.modifyOrder(msg.orderId, msg.newPrice, msg.newQuantity, msg.participantId);
    if (success) {
        uint64_t timestamp = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
        auto trades = orderBook.matchBook(nextSequence.load(), timestamp);
        for (auto &t : trades) {
            nextSequence.store(t.header.sequence + 1);
            sendExecution(t);
        }
    }
    return success;
}

void MatchingEngine::processSnapshotRequest(const SnapshotRequest &msg) {
    std::shared_lock<std::shared_mutex> lock(orderBook.bookMutex);
    SnapshotResponse resp;
    resp.header.type = MessageType::SNAPSHOT_RESPONSE;
    resp.header.sequence = msg.header.sequence;
    resp.header.timestamp = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
    resp.symbol = msg.symbol;
    orderBook.getTopOfBook(resp.bestBid, resp.bestAsk);
    resp.lastTradePrice = orderBook.getLastTradePrice();
    LOG(LogLevel::INFO, "Snapshot for " << msg.symbol << ": bestBid=" << resp.bestBid << ", bestAsk=" << resp.bestAsk);
}

void MatchingEngine::sendExecution(const ExecutionMessage &exec) {
    // Multicast execution
    replayLog.logExecutionMessage(exec.header.sequence, exec);
    LOG(LogLevel::INFO, "Execution: seq=" << exec.header.sequence << " symbol=" << exec.symbol << " qty=" << exec.quantity << " price=" << exec.price);
}

void MatchingEngine::step() {
    // All ops triggered by client request so no delayed orders
}

bool MatchingEngine::priceValidForSymbol(const std::string &symbol, double price) {
    SymbolConfig cfg;
    if (!configManager.getConfig(symbol, cfg)) return false;
    return (price >= cfg.minPrice && price <= cfg.maxPrice);
}

bool MatchingEngine::tickSizeValid(const std::string &symbol, double price) {
    SymbolConfig cfg;
    if (!configManager.getConfig(symbol, cfg)) return false;
    double ticks = price / cfg.tickSize;
    double rounded = std::floor(ticks);
    double diff = std::abs(ticks - rounded);
    return diff < 1e-9; // close enough for floating point
}

bool MatchingEngine::quantityValid(const std::string &symbol, uint64_t qty) {
    SymbolConfig cfg;
    if (!configManager.getConfig(symbol, cfg)) return false;
    return qty >= cfg.minQuantity;
}

bool MatchingEngine::checkVolatilityHalt(const AddMessage &msg) {
    SymbolConfig cfg;
    if (!configManager.getConfig(msg.symbol, cfg)) return false;
    if (cfg.tradingHalted) return true;
    // Simple check: if price is way off reference
    if (msg.orderType == OrderType::LIMIT || msg.orderType == OrderType::ICEBERG) {
        double pctChange = std::abs((msg.price - cfg.referencePrice)/cfg.referencePrice);
        if (pctChange > cfg.volatilityThreshold) {
            // Trigger halt
            configManager.haltTrading(msg.symbol);
            return true;
        }
    }
    return false;
}

bool MatchingEngine::checkTimeInForce(Order* o, std::vector<ExecutionMessage> &trades, uint64_t timestamp) {
    // If FOK and not fully matched, cancel:
    if (o->tif == TimeInForce::FOK) {
        if (o->quantity > 0) {
            // not fully matched
            orderBook.cancelOrder(o->orderId, o->participantId);
        }
        return false; // order done
    } else if (o->tif == TimeInForce::IOC) {
        // IOC: cancel remainder
        if (o->quantity > 0) {
            orderBook.cancelOrder(o->orderId, o->participantId);
        }
        return false;
    }
    return true; // GTC continues
}

void MatchingEngine::handleMarketOrder(Order* o, std::vector<ExecutionMessage> &trades, uint64_t timestamp) {
    std::unique_lock<std::shared_mutex> lock(orderBook.bookMutex);
    orderBook.addOrder(o); // Add to lookup to allow cancel if needed
    // Immediately match
    auto res = orderBook.matchBook(nextSequence.load(), timestamp);

    // Market orders are either fully matched or partial
    // If partial remains, and TIF=FOK or IOC, handle
    // If GTC (which doesn't make sense for market in this design), we would just discard remainder.

    for (auto &t : res) {
        trades.push_back(t);
    }

    // For a market order without FOK/IOC explicitly set, treat as FOK?
    // Let's assume market orders always IOC.
    // Cancel remainder if any
    if (o->quantity > 0) {
        orderBook.cancelOrder(o->orderId, o->participantId);
    }
}

void MatchingEngine::handleIocFok(Order* o, std::vector<ExecutionMessage> &trades, uint64_t timestamp) {
    std::unique_lock<std::shared_mutex> lock(orderBook.bookMutex);
    orderBook.addOrder(o);
    auto res = orderBook.matchBook(nextSequence.load(), timestamp);
    for (auto &t : res) {
        trades.push_back(t);
    }
    checkTimeInForce(o, trades, timestamp);
}

