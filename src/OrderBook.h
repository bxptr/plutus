#pragma once
#include <map>
#include <queue>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include "Messages.h"
#include "MemoryPool.h"
#include "Order.h"
#include "Logging.h"

// OrderBook now also maintains stop and iceberg orders.
// Stop-loss orders are stored in a separate structure and activated when price triggers.
// Iceberg orders are stored like normal orders but manage visibleQuantity internally.

class OrderBook {
public:
    OrderBook();

    bool addOrder(Order* o);
    bool cancelOrder(uint64_t orderId, uint64_t participantId);
    bool modifyOrder(uint64_t orderId, double newPrice, uint64_t newQty, uint64_t participantId);

    std::vector<ExecutionMessage> match(uint64_t seqBase, uint64_t timestamp);

    void getTopOfBook(double &bestBid, double &bestAsk);
    void setMemoryPool(MemoryPool<Order>* pool) { orderPool_ = pool; }

    // Add a trade price to track volatility
    double getLastTradePrice();
    void recordTradePrice(double price);

    // Trigger stop-loss orders if conditions are met
    void triggerStopOrders(uint64_t timestamp, uint64_t &seqBase);

private:
    // For simplicity: bids: descending price, asks: ascending price
    std::map<double, std::queue<Order*>> bids;
    std::map<double, std::queue<Order*>> asks;

    std::unordered_map<uint64_t, Order*> orderLookup;

    // Stop-loss orders: store separately keyed by trigger price and side
    // On trigger, convert them into market orders
    // We use a multimap keyed by triggerPrice, to quickly find triggers
    std::multimap<double, Order*> stopOrdersBuy;  // trigger when price <= triggerPrice
    std::multimap<double, Order*> stopOrdersSell; // trigger when price >= triggerPrice

    // Access control
    std::shared_mutex bookMutex;
    MemoryPool<Order>* orderPool_ = nullptr;

    // Internal utilities
    bool removeOrderFromBook(Order* o);
    void insertStopOrder(Order* o);
    void activateStopOrder(Order* o, uint64_t timestamp, uint64_t &seqBase, std::vector<ExecutionMessage> &trades);

    std::vector<ExecutionMessage> matchBook(uint64_t seqBase, uint64_t timestamp);

    // For volatility tracking
    double lastTradePrice = 0.0;
    bool haveLastTrade = false;
    std::deque<std::pair<double, uint64_t>> recentTrades; // Price, Quantity
    size_t maxRecentTrades = 100; // Maintain last 100 trades

    // Helper for iceberg orders: refresh visible qty after partial fills
    void refreshIceberg(Order* o);

    friend class MatchingEngine;
};
