#include "EventLoop.h"
#include "Logging.h"
#include "NetworkInterface.h"
#include <sys/event.h>
#include <unistd.h>
#include <errno.h>

EventLoop::EventLoop(EngineController &controller)
    : controller_(controller) {}

bool EventLoop::init(int listenFd) {
    listenFd_ = listenFd;
    kqfd_ = kqueue();
    if (kqfd_ < 0) {
        LOG(LogLevel::ERROR, "kqueue creation failed");
        return false;
    }

    struct kevent ev;
    EV_SET(&ev, listenFd_, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    if (kevent(kqfd_, &ev, 1, nullptr, 0, nullptr) < 0) {
        LOG(LogLevel::ERROR, "kevent ADD listenFd failed");
        return false;
    }
    return true;
}

void EventLoop::run() {
    const int MAX_EVENTS = 64;
    struct kevent events[MAX_EVENTS];

    while (true) {
        int n = kevent(kqfd_, nullptr, 0, events, MAX_EVENTS, nullptr);
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG(LogLevel::ERROR, "kevent wait error");
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = (int)events[i].ident;
            int16_t filter = events[i].filter;

            if (fd == listenFd_) {
                if (!handleNewConnection()) {
                    LOG(LogLevel::ERROR, "handleNewConnection failed");
                }
            } else {
                if (!handleEvent(fd, filter)) {
                    removeSession(fd);
                }
            }
        }
    }
}

bool EventLoop::handleNewConnection() {
    NetworkInterface net;
    std::string addr;
    int clientFd = net.acceptClient(listenFd_, addr);
    if (clientFd < 0) {
        return true;
    }

    Session *sess = new Session(clientFd, controller_, kqfd_);

    struct kevent ev;
    EV_SET(&ev, clientFd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, nullptr);
    if (kevent(kqfd_, &ev, 1, nullptr, 0, nullptr) < 0) {
        LOG(LogLevel::ERROR, "kevent ADD clientFd failed");
        delete sess;
        return false;
    }

    sessions_[clientFd] = sess;
    LOG(LogLevel::INFO, "New client connected fd=" << clientFd);
    return true;
}

bool EventLoop::handleEvent(int fd, int16_t filter) {
    auto it = sessions_.find(fd);
    if (it == sessions_.end()) {
        LOG(LogLevel::WARN, "Event for unknown fd");
        return false;
    }
    Session* sess = it->second;

    if (filter == EVFILT_READ) {
        if (!sess->onReadable()) return false;
    } else if (filter == EVFILT_WRITE) {
        if (!sess->onWritable()) return false;
    }

    return true;
}

void EventLoop::removeSession(int fd) {
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kqfd_, &ev, 1, nullptr, 0, nullptr);
    
    auto it = sessions_.find(fd);
    if (it != sessions_.end()) {
        delete it->second;
        sessions_.erase(it);
    }
    close(fd);
    LOG(LogLevel::INFO, "Client disconnected fd=" << fd);
}

