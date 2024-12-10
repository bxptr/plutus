#include "MessageParser.h"
#include "Logging.h"
#include <algorithm>
#include <string>
#include <sstream>

void MessageParser::appendData(const char* data, size_t len) {
    buffer_.insert(buffer_.end(), data, data + len);
}

std::optional<MessageHeader> MessageParser::nextMessageHeader() {
    auto it = std::find(buffer_.begin(), buffer_.end(), '\n');
    if (it == buffer_.end()) {
        return std::nullopt;
    }
    // Peek line
    std::string line(buffer_.begin(), it);
    // Format: TYPE|seq|timestamp|...
    // Extract first three fields
    std::istringstream iss(line);
    std::string typeStr; 
    std::getline(iss, typeStr, '|');
    if (typeStr.empty()) {
        // consume line
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }

    std::string seqStr;
    std::getline(iss, seqStr, '|');
    if (seqStr.empty()) {
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }

    std::string tsStr;
    std::getline(iss, tsStr, '|');
    if (tsStr.empty()) {
        // Some messages might not have more fields
        // Just handle minimal
        tsStr = "0";
    }

    MessageType mt;
    if (typeStr == "ADD") mt = MessageType::ADD;
    else if (typeStr == "CANCEL") mt = MessageType::CANCEL;
    else if (typeStr == "CANCEL_REPLACE") mt = MessageType::CANCEL_REPLACE;
    else if (typeStr == "SNAPSHOT_REQUEST") mt = MessageType::SNAPSHOT_REQUEST;
    else {
        // unknown
        LOG(LogLevel::WARN, "Unknown message type: " << typeStr);
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }

    uint64_t seq = std::stoull(seqStr);
    uint64_t ts = std::stoull(tsStr);

    MessageHeader hdr{mt, seq, ts};
    return hdr;
}

static std::vector<std::string> splitLine(const std::string &line) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        auto pos = line.find('|', start);
        if (pos == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        } else {
            parts.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }
    }
    return parts;
}

std::optional<AddMessage> MessageParser::nextAddMessage() {
    auto it = std::find(buffer_.begin(), buffer_.end(), '\n');
    if (it == buffer_.end()) return std::nullopt;
    std::string line(buffer_.begin(), it);

    auto parts = splitLine(line);
    // ADD|seq|ts|orderId|symbol|price|qty|side|tif|ordertype|participantId|triggerPrice|visibleQty
    // Side=BUY/SELL, tif=GTC/IOC/FOK, ordertype=LIMIT/MARKET/STOP_LOSS/ICEBERG
    if (parts.size() < 8) {
        // consume line
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }
    if (parts[0] != "ADD") {
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }
    AddMessage msg;
    msg.header.type = MessageType::ADD;
    msg.header.sequence = std::stoull(parts[1]);
    msg.header.timestamp = std::stoull(parts[2]);
    msg.orderId = std::stoull(parts[3]);
    msg.symbol = parts[4];
    msg.price = std::stod(parts[5]);
    msg.quantity = std::stoull(parts[6]);
    msg.side = (parts[7] == "BUY") ? Side::BUY : Side::SELL;

    // Default
    msg.tif = TimeInForce::GTC;
    msg.orderType = OrderType::LIMIT;
    msg.participantId = 0;
    msg.triggerPrice = 0.0;
    msg.visibleQuantity = msg.quantity;

    if (parts.size() >= 9) {
        std::string tifStr = parts[8];
        if (tifStr == "GTC") msg.tif = TimeInForce::GTC;
        else if (tifStr == "IOC") msg.tif = TimeInForce::IOC;
        else if (tifStr == "FOK") msg.tif = TimeInForce::FOK;
    }

    if (parts.size() >= 10) {
        std::string otype = parts[9];
        if (otype == "LIMIT") msg.orderType = OrderType::LIMIT;
        else if (otype == "MARKET") msg.orderType = OrderType::MARKET;
        else if (otype == "STOP_LOSS") msg.orderType = OrderType::STOP_LOSS;
        else if (otype == "ICEBERG") msg.orderType = OrderType::ICEBERG;
    }

    if (parts.size() >= 11) {
        msg.participantId = std::stoull(parts[10]);
    }

    if (parts.size() >= 12) {
        msg.triggerPrice = std::stod(parts[11]);
    }

    if (parts.size() >= 13) {
        msg.visibleQuantity = std::stoull(parts[12]);
    }

    buffer_.erase(buffer_.begin(), it+1);
    return msg;
}

std::optional<CancelMessage> MessageParser::nextCancelMessage() {
    auto it = std::find(buffer_.begin(), buffer_.end(), '\n');
    if (it == buffer_.end()) return std::nullopt;
    std::string line(buffer_.begin(), it);
    auto parts = splitLine(line);
    // CANCEL|seq|ts|orderId|participantId
    if (parts.size() < 4 || parts[0] != "CANCEL") {
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }

    CancelMessage msg;
    msg.header.type = MessageType::CANCEL;
    msg.header.sequence = std::stoull(parts[1]);
    msg.header.timestamp = std::stoull(parts[2]);
    msg.orderId = std::stoull(parts[3]);
    msg.participantId = 0;
    if (parts.size() >= 5) {
        msg.participantId = std::stoull(parts[4]);
    }

    buffer_.erase(buffer_.begin(), it+1);
    return msg;
}

std::optional<CancelReplaceMessage> MessageParser::nextCancelReplaceMessage() {
    auto it = std::find(buffer_.begin(), buffer_.end(), '\n');
    if (it == buffer_.end()) return std::nullopt;
    std::string line(buffer_.begin(), it);
    auto parts = splitLine(line);
    // CANCEL_REPLACE|seq|ts|orderId|newPrice|newQty|participantId
    if (parts.size() < 6 || parts[0] != "CANCEL_REPLACE") {
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }
    CancelReplaceMessage msg;
    msg.header.type = MessageType::CANCEL_REPLACE;
    msg.header.sequence = std::stoull(parts[1]);
    msg.header.timestamp = std::stoull(parts[2]);
    msg.orderId = std::stoull(parts[3]);
    msg.newPrice = std::stod(parts[4]);
    msg.newQuantity = std::stoull(parts[5]);
    msg.participantId = 0;
    if (parts.size() >= 7) {
        msg.participantId = std::stoull(parts[6]);
    }

    buffer_.erase(buffer_.begin(), it+1);
    return msg;
}

std::optional<SnapshotRequest> MessageParser::nextSnapshotRequest() {
    auto it = std::find(buffer_.begin(), buffer_.end(), '\n');
    if (it == buffer_.end()) return std::nullopt;
    std::string line(buffer_.begin(), it);
    auto parts = splitLine(line);
    // SNAPSHOT_REQUEST|seq|ts|symbol
    if (parts.size() < 4 || parts[0] != "SNAPSHOT_REQUEST") {
        buffer_.erase(buffer_.begin(), it+1);
        return std::nullopt;
    }

    SnapshotRequest msg;
    msg.header.type = MessageType::SNAPSHOT_REQUEST;
    msg.header.sequence = std::stoull(parts[1]);
    msg.header.timestamp = std::stoull(parts[2]);
    msg.symbol = parts[3];

    buffer_.erase(buffer_.begin(), it+1);
    return msg;
}

