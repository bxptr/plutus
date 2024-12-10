#pragma once
#include <string>
#include <cstdint>

enum class MessageType {
    ADD,
    CANCEL,
    CANCEL_REPLACE,
    EXECUTION,
    SNAPSHOT_REQUEST,
    SNAPSHOT_RESPONSE,
    HEARTBEAT
};

enum class Side : uint8_t { BUY, SELL };

enum class TimeInForce : uint8_t {
    GTC,  // Good Till Cancel
    IOC,  // Immediate Or Cancel
    FOK   // Fill Or Kill
};

enum class OrderType : uint8_t {
    LIMIT,
    MARKET,
    STOP_LOSS,
    ICEBERG
};

struct MessageHeader {
    MessageType type;
    uint64_t sequence;
    uint64_t timestamp;
};

struct AddMessage {
    MessageHeader header;
    uint64_t orderId;
    std::string symbol;
    double price;
    uint64_t quantity;
    Side side;
    TimeInForce tif;
    OrderType orderType;
    uint64_t participantId;
    double triggerPrice;      // For STOP_LOSS
    uint64_t visibleQuantity; // For ICEBERG
};

struct CancelMessage {
    MessageHeader header;
    uint64_t orderId;
    uint64_t participantId; // for validation
};

struct CancelReplaceMessage {
    MessageHeader header;
    uint64_t orderId;
    double newPrice;
    uint64_t newQuantity;
    uint64_t participantId;
};

struct ExecutionMessage {
    MessageHeader header;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    char symbol[8];
    double price;
    uint64_t quantity;
    uint64_t buyParticipantId;
    uint64_t sellParticipantId;
};

struct SnapshotRequest {
    MessageHeader header;
    std::string symbol;
};

struct SnapshotResponse {
    MessageHeader header;
    std::string symbol;
    // Snapshot details: top of book, etc.
    double bestBid;
    double bestAsk;
    // Could add more levels later but not required for now
};
