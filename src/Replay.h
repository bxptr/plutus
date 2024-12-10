#pragma once
#include <mutex>
#include "Messages.h"
#include "Logging.h"
#include <fstream>

class Replay {
public:
    Replay();
    void logAddMessage(uint64_t seq, const AddMessage &msg);
    void logCancelMessage(uint64_t seq, const CancelMessage &msg);
    void logCancelReplaceMessage(uint64_t seq, const CancelReplaceMessage &msg);
    void logExecutionMessage(uint64_t seq, const ExecutionMessage &msg);

    void replayAll();

private:
    std::mutex mtx_;
    std::ofstream logfile_;
};
