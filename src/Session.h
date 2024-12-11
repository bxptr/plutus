#pragma once
#include <string>
#include <vector>
#include "MessageParser.h"
#include "EngineController.h"

class Session {
public:
    Session(int fd, EngineController &controller, int kqfd_);
    ~Session();

    int getFd() const { return fd_; }

    bool onReadable();
    bool onWritable();
    void queueResponse(const std::string &msg);

private:
    int kqfd_;
    int fd_;
    std::string clientAddr_;
    EngineController &controller_;

    std::vector<std::string> writeQueue_;
    MessageParser parser_;

    bool handleAdd(const AddMessage &msg);
    bool handleCancel(const CancelMessage &msg);
    bool handleCancelReplace(const CancelReplaceMessage &msg);
    bool handleSnapshotRequest(const SnapshotRequest &msg);
};

