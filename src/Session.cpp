#include "Session.h"
#include "Logging.h"
#include <unistd.h>

Session::Session(int fd, EngineController &controller)
    : fd_(fd), controller_(controller) { }

Session::~Session() {
    close(fd_);
}

bool Session::onReadable() {
    char buf[4096];
    while (true) {
        ssize_t n = read(fd_, buf, sizeof(buf));
        if (n > 0) {
            parser_.appendData(buf, n);
            while (true) {
                auto hdr = parser_.nextMessageHeader();
                if (!hdr.has_value()) break; // need more data
                MessageType mt = hdr->type;

                bool handled = true;
                if (mt == MessageType::ADD) {
                    auto m = parser_.nextAddMessage();
                    if (!m.has_value()) break; 
                    handled = handleAdd(*m);
                } else if (mt == MessageType::CANCEL) {
                    auto m = parser_.nextCancelMessage();
                    if (!m.has_value()) break;
                    handled = handleCancel(*m);
                } else if (mt == MessageType::CANCEL_REPLACE) {
                    auto m = parser_.nextCancelReplaceMessage();
                    if (!m.has_value()) break;
                    handled = handleCancelReplace(*m);
                } else if (mt == MessageType::SNAPSHOT_REQUEST) {
                    auto m = parser_.nextSnapshotRequest();
                    if (!m.has_value()) break;
                    handled = handleSnapshotRequest(*m);
                } else {
                    LOG(LogLevel::WARN, "Unknown message type");
                    handled = false;
                    break;
                }

                if (!handled) {
                    LOG(LogLevel::ERROR, "Failed to handle message");
                }
            }
        } else if (n == 0) {
            // client disconnected
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // no more data
                break;
            } else {
                LOG(LogLevel::ERROR, "read error on client fd");
                return false;
            }
        }
    }

    return true;
}

bool Session::onWritable() {
    while (!writeQueue_.empty()) {
        const std::string &msg = writeQueue_.front();
        ssize_t n = write(fd_, msg.data(), msg.size());
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            } else {
                LOG(LogLevel::ERROR, "write error");
                return false;
            }
        } else if ((size_t)n < msg.size()) {
            writeQueue_.front() = msg.substr(n);
            return true;
        } else {
            writeQueue_.erase(writeQueue_.begin());
        }
    }
    return true;
}

void Session::queueResponse(const std::string &msg) {
    writeQueue_.push_back(msg);
}

bool Session::handleAdd(const AddMessage &msg) {
    bool success = controller_.dispatchAdd(msg);
    if (success) queueResponse("ADD_ACK\n");
    else queueResponse("ADD_NACK\n");
    return success;
}

bool Session::handleCancel(const CancelMessage &msg) {
    bool success = controller_.dispatchCancel(msg);
    if (success) queueResponse("CANCEL_ACK\n");
    else queueResponse("CANCEL_NACK\n");
    return success;
}

bool Session::handleCancelReplace(const CancelReplaceMessage &msg) {
    bool success = controller_.dispatchCancelReplace(msg);
    if (success) queueResponse("CANCEL_REPLACE_ACK\n");
    else queueResponse("CANCEL_REPLACE_NACK\n");
    return success;
}

bool Session::handleSnapshotRequest(const SnapshotRequest &msg) {
    controller_.dispatchSnapshotRequest(msg);
    queueResponse("SNAPSHOT_SENT\n");
    return true;
}
