#include "OrderBook.h"
#include "Logging.h"
#include <algorithm>
#include <chrono>

OrderBook::OrderBook() {}

bool OrderBook::addOrder(Order* o) {
    if (!o) { LOG(LogLevel::ERROR, "addOrder: Null order pointer"); return false; }
    if (orderLookup.find(o->orderId) != orderLookup.end()) {
        LOG(LogLevel::WARN, "addOrder: orderId already exists");
        return false;
    }

    if (o->orderType == OrderType::STOP_LOSS) {
        insertStopOrder(o);
        orderLookup[o->orderId] = o;
        return true;
    }

    if (o->orderType == OrderType::MARKET) {
        // Market orders won't rest in the book.
        // They will be matched immediately by the caller.
        // Just add to lookup so we can cancel if needed quickly (FOK scenario)
        orderLookup[o->orderId] = o;
        return true;
    }

    auto &book = (o->side == Side::BUY) ? bids : asks;
    book[o->price].push(o);
    orderLookup[o->orderId] = o;
    return true;
}

bool OrderBook::cancelOrder(uint64_t orderId, uint64_t participantId) {
    auto it = orderLookup.find(orderId);
    if (it == orderLookup.end()) {
        LOG(LogLevel::INFO, "cancelOrder: orderId not found");
        return false;
    }
    Order* o = it->second;
    if (o->participantId != participantId) {
        LOG(LogLevel::WARN, "cancelOrder: participant mismatch");
        return false; // not allowed to cancel others' orders
    }

    if (o->orderType == OrderType::STOP_LOSS) {
        // Remove from stopOrders maps
        bool found = false;
        if (o->side == Side::BUY) {
            auto range = stopOrdersBuy.equal_range(o->triggerPrice);
            for (auto sit = range.first; sit != range.second; ++sit) {
                if (sit->second == o) {
                    stopOrdersBuy.erase(sit);
                    found = true;
                    break;
                }
            }
        } else {
            auto range = stopOrdersSell.equal_range(o->triggerPrice);
            for (auto sit = range.first; sit != range.second; ++sit) {
                if (sit->second == o) {
                    stopOrdersSell.erase(sit);
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            LOG(LogLevel::WARN, "cancelOrder: stop order not found in stopOrders map");
        }
        orderLookup.erase(orderId);
        orderPool_->deallocate(o);
        return true;
    } else if (o->orderType == OrderType::MARKET) {
        // If market order is still here, means FOK/IOC scenario
        orderLookup.erase(orderId);
        orderPool_->deallocate(o);
        return true;
    }

    bool removed = removeOrderFromBook(o);
    if (removed && orderPool_) {
        orderPool_->deallocate(o);
    }
    return removed;
}

bool OrderBook::modifyOrder(uint64_t orderId, double newPrice, uint64_t newQty, uint64_t participantId) {
    auto it = orderLookup.find(orderId);
    if (it == orderLookup.end()) {
        LOG(LogLevel::INFO, "modifyOrder: orderId not found");
        return false;
    }
    Order* oldOrder = it->second;
    if (oldOrder->participantId != participantId) {
        LOG(LogLevel::WARN, "modifyOrder: participant mismatch");
        return false;
    }

    // Can't modify stop loss trigger conditions or order type easily here; we keep it simple
    // We'll allow price/qty modifications for limit orders. For stop or market, disallow.
    if (oldOrder->orderType != OrderType::LIMIT && oldOrder->orderType != OrderType::ICEBERG) {
        LOG(LogLevel::WARN, "modifyOrder: can only modify limit/iceberg");
        return false;
    }

    if (!removeOrderFromBook(oldOrder)) {
        return false;
    }

    oldOrder->price = newPrice;
    oldOrder->quantity = newQty;
    oldOrder->visibleQuantity = (oldOrder->orderType == OrderType::ICEBERG && oldOrder->visibleQuantity > newQty) ? newQty : oldOrder->visibleQuantity;
    oldOrder->totalQuantity = newQty;

    auto &book = (oldOrder->side == Side::BUY) ? bids : asks;
    book[newPrice].push(oldOrder);
    orderLookup[oldOrder->orderId] = oldOrder;
    return true;
}

bool OrderBook::removeOrderFromBook(Order* o) {
    auto &book = (o->side == Side::BUY) ? bids : asks;
    auto it = book.find(o->price);
    if (it == book.end()) return false;

    std::queue<Order*> &q = it->second;
    std::queue<Order*> newQ;
    bool removed = false;
    while(!q.empty()) {
        Order* front = q.front(); q.pop();
        if (front == o) {
            removed = true;
        } else {
            newQ.push(front);
        }
    }
    if (newQ.empty()) {
        book.erase(it);
    } else {
        it->second = std::move(newQ);
    }
    if (removed) {
        orderLookup.erase(o->orderId);
    }
    return removed;
}

void OrderBook::getTopOfBook(double &bestBid, double &bestAsk) {
    std::shared_lock<std::shared_mutex> lock(bookMutex);
    bestBid = (bids.empty()) ? 0.0 : bids.rbegin()->first;
    bestAsk = (asks.empty()) ? 0.0 : asks.begin()->first;
}

std::vector<ExecutionMessage> OrderBook::match(uint64_t seqBase, uint64_t timestamp) {
    std::unique_lock<std::shared_mutex> lock(bookMutex);
    // Trigger stop orders if needed
    triggerStopOrders(timestamp, seqBase);
    return matchBook(seqBase, timestamp);
}

std::vector<ExecutionMessage> OrderBook::matchBook(uint64_t seqBase, uint64_t timestamp) {
    std::vector<ExecutionMessage> trades;
    while(!bids.empty() && !asks.empty()) {
        double bestBid = bids.rbegin()->first;
        double bestAsk = asks.begin()->first;
        if (bestBid < bestAsk) break;

        auto &bidQueue = bids.rbegin()->second;
        auto &askQueue = asks.begin()->second;
        Order* bidOrder = bidQueue.front();
        Order* askOrder = askQueue.front();

        // Prevent self-trade
        if (bidOrder->participantId == askOrder->participantId) {
            // If same participant, we cannot match these two.
            // Move on by removing or skipping one side. Typically you'd just break.
            // We'll just break as no trade is possible at this price level.
            break;
        }

        uint64_t tradeQty = std::min(bidOrder->quantity, askOrder->quantity);
        double tradePrice = askOrder->price; // trades at passive order price

        ExecutionMessage exec;
        exec.header.type = MessageType::EXECUTION;
        exec.header.sequence = seqBase++;
        exec.header.timestamp = timestamp;
        exec.buyOrderId = (bidOrder->side == Side::BUY) ? bidOrder->orderId : askOrder->orderId;
        exec.sellOrderId = (askOrder->side == Side::SELL) ? askOrder->orderId : bidOrder->orderId;
        for (int i=0; i<8; i++) exec.symbol[i] = bidOrder->symbol[i];
        exec.price = tradePrice;
        exec.quantity = tradeQty;
        exec.buyParticipantId = bidOrder->participantId;
        exec.sellParticipantId = askOrder->participantId;
        trades.push_back(exec);

        bidOrder->quantity -= tradeQty;
        askOrder->quantity -= tradeQty;

        recordTradePrice(tradePrice, tradeQty);
        if (bidOrder->orderType == OrderType::ICEBERG) refreshIceberg(bidOrder);
        if (askOrder->orderType == OrderType::ICEBERG) refreshIceberg(askOrder);

        if (bidOrder->quantity == 0) {
            bidQueue.pop();
            orderLookup.erase(bidOrder->orderId);
            if (orderPool_) orderPool_->deallocate(bidOrder);
        }

        if (askOrder->quantity == 0) {
            askQueue.pop();
            orderLookup.erase(askOrder->orderId);
            if (orderPool_) orderPool_->deallocate(askOrder);
        }

        if (bidQueue.empty()) {
            auto bidIter = bids.end(); 
            --bidIter;
            bids.erase(bidIter->first);
        }

        if (askQueue.empty()) {
            asks.erase(asks.begin()->first);
        }
    }
    return trades;
}

double OrderBook::getLastTradePrice() const {
    std::shared_lock<std::shared_mutex> lock(bookMutex);
    // Use a Volume-Weighted Average Price
    double totalValue = 0.0;
    uint64_t totalVolume = 0;
    for (const auto &[price, quantity] : recentTrades) {
        totalValue += price * quantity;
        totalVolume += quantity;
    }
    return (totalVolume > 0) ? (totalValue / totalVolume) : 0.0;
}

void OrderBook::recordTradePrice(double price, uint64_t quantity) {
    std::unique_lock<std::shared_mutex> lock(bookMutex);
    recentTrades.emplace_back(price, quantity);
    if (recentTrades.size() > maxRecentTrades) {
        recentTrades.pop_front();
    }
}

void OrderBook::triggerStopOrders(uint64_t timestamp, uint64_t &seqBase) {
    // If we have lastTradePrice, trigger STOP_LOSS orders
    if (!haveLastTrade) return;

    std::vector<Order*> toActivate;
    // Buy stop: trigger when lastTradePrice <= triggerPrice
    // We'll find all buy stops with triggerPrice >= lastTradePrice
    {
        auto it = stopOrdersBuy.lower_bound(lastTradePrice);
        // Need to activate all orders with triggerPrice >= lastTradePrice
        // Actually for buy stop-loss orders, usually they trigger if price breaks below a threshold.
        // Here we interpret stop-loss buy: triggers if lastPrice >= triggerPrice or lastPrice <= triggerPrice?
        // Typically a stop BUY triggers if market goes above trigger (for a breakout).
        // A stop SELL triggers if market goes below trigger.
        // Let's define:
        // Stop BUY triggers if lastTradePrice >= triggerPrice
        // Stop SELL triggers if lastTradePrice <= triggerPrice
        // We'll adjust logic:
    }

    // Re-interpret:
    // STOP_LOSS BUY: Activate if lastTradePrice >= triggerPrice
    // STOP_LOSS SELL: Activate if lastTradePrice <= triggerPrice

    // Activate buy stops where triggerPrice <= lastTradePrice
    {
        // For buy stops: triggers if lastTradePrice >= triggerPrice
        // So we want all orders in stopOrdersBuy with triggerPrice <= lastTradePrice
        for (auto it = stopOrdersBuy.begin(); it != stopOrdersBuy.end();) {
            if (it->first <= lastTradePrice) {
                toActivate.push_back(it->second);
                it = stopOrdersBuy.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Activate sell stops where triggerPrice >= lastTradePrice
    {
        // Sell stops trigger if lastTradePrice <= triggerPrice
        // Means if their triggerPrice >= lastTradePrice, we activate them
        for (auto it = stopOrdersSell.begin(); it != stopOrdersSell.end();) {
            if (it->first >= lastTradePrice) {
                toActivate.push_back(it->second);
                it = stopOrdersSell.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::vector<ExecutionMessage> dummyTrades;
    for (auto* o : toActivate) {
        activateStopOrder(o, timestamp, seqBase, dummyTrades);
    }

    // Now that stop orders are activated as market orders, they will match on the next match call
    // Actually, we can immediately attempt to match them by calling matchBook again
    // but that is done by the caller after triggerStopOrders.
}

void OrderBook::activateStopOrder(Order* o, uint64_t timestamp, uint64_t &seqBase, std::vector<ExecutionMessage> &trades) {
    // Turn stop-loss order into a market order (or limit if we prefer)
    // For simplicity, stop orders become market orders when triggered
    o->orderType = OrderType::MARKET;
    // Insert into lookup if not present
    orderLookup[o->orderId] = o;
    // They will be matched later (caller will run matchBook)
    // Market orders do not rest in book, matching engine handles them immediately.
    // This function just changes their type. The engine call after this should handle them.
}

void OrderBook::insertStopOrder(Order* o) {
    if (o->side == Side::BUY) {
        stopOrdersBuy.insert({o->triggerPrice, o});
    } else {
        stopOrdersSell.insert({o->triggerPrice, o});
    }
}

void OrderBook::refreshIceberg(Order* o) {
    if (o->orderType != OrderType::ICEBERG) return;

    // If visibleQuantity is depleted but totalQuantity still > quantity, we can refresh visible.
    uint64_t alreadyVisible = o->totalQuantity - o->quantity;
    if (o->quantity < o->visibleQuantity && o->quantity > 0) {
        // visibleQuantity can't exceed current quantity
        // Actually, iceberg logic: visibleQuantity sets how much is shown. If partially filled,
        // once visible is depleted, we show again up to visibleQuantity, but not exceeding total left.
        // If currently quantity < visibleQuantity, no need to do anything, it's already less visible.
        return;
    } else if (o->quantity > o->visibleQuantity) {
        // It's possible that after trade, full visible was taken. We still have more hidden qty.
        // Actually, we only refresh iceberg after a fill reduces visible to 0.
        // If quantity still >= visibleQuantity, we do nothing special.
        return;
    }
    // If the order was partially filled and now quantity < visibleQuantity,
    // that means no refresh needed. If quantity =0, order is gone anyway.
}

