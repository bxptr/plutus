#pragma once
#include <cstdint>
#include <string>
#include "Messages.h"

struct Order {
    uint64_t orderId;
    Side side;
    char symbol[8];
    double price;
    uint64_t quantity;
    uint64_t timestamp;
    uint64_t participantId;
    TimeInForce tif;
    OrderType orderType;
    double triggerPrice;      
    uint64_t visibleQuantity;
    uint64_t totalQuantity; // For iceberg: total initial qty

    Order(uint64_t id, Side s, const std::string &sym, double p, uint64_t q, uint64_t ts,
          uint64_t partId, TimeInForce t, OrderType otype, double trigP, uint64_t visQty)
        : orderId(id), side(s), price(p), quantity(q), timestamp(ts),
          participantId(partId), tif(t), orderType(otype), triggerPrice(trigP),
          visibleQuantity(visQty), totalQuantity(q) {
        for (size_t i = 0; i<7 && i<sym.size(); i++) {
            symbol[i] = sym[i];
        }
        symbol[std::min<size_t>(sym.size(),7)] = '\0';
    }

    Order() = default;
};

