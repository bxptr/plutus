#pragma once
#include "Messages.h"
#include <vector>
#include <optional>

class MessageParser {
public:
    void appendData(const char* data, size_t len);
    std::optional<MessageHeader> nextMessageHeader();
    std::optional<AddMessage> nextAddMessage();
    std::optional<CancelMessage> nextCancelMessage();
    std::optional<CancelReplaceMessage> nextCancelReplaceMessage();
    std::optional<SnapshotRequest> nextSnapshotRequest();

private:
    std::vector<char> buffer_;
};

