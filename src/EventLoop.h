#pragma once
#include <unordered_map>
#include "Session.h"
#include "EngineController.h"

class EventLoop {
public:
    EventLoop(EngineController &controller);

    bool init(int listenFd);
    void run();

private:
    int kqfd_ = -1;
    int listenFd_ = -1;
    EngineController &controller_;
    std::unordered_map<int, Session*> sessions_;

    bool handleNewConnection();
    bool handleEvent(int fd, int16_t filter);
    void removeSession(int fd);
};
;
