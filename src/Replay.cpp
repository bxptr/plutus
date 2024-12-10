#include "Replay.h"

Replay::Replay() {
    logfile_.open("replay.log", std::ios::app);
}

void Replay::logAddMessage(uint64_t seq, const AddMessage &msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    logfile_ << "ADD|" << seq << "|" << msg.orderId << "|" << msg.symbol << "|" << msg.price << "|" << msg.quantity << "\n";
}

void Replay::logCancelMessage(uint64_t seq, const CancelMessage &msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    logfile_ << "CANCEL|" << seq << "|" << msg.orderId << "\n";
}

void Replay::logCancelReplaceMessage(uint64_t seq, const CancelReplaceMessage &msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    logfile_ << "CANCEL_REPLACE|" << seq << "|" << msg.orderId << "|" << msg.newPrice << "|" << msg.newQuantity << "\n";
}

void Replay::logExecutionMessage(uint64_t seq, const ExecutionMessage &msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    logfile_ << "EXEC|" << seq << "|" << msg.symbol << "|" << msg.price << "|" << msg.quantity << "\n";
}

void Replay::replayAll() {
    // Read replay.log and re-apply messages to rebuild state but not required yet.
}

