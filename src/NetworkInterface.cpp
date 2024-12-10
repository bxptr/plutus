#include "NetworkInterface.h"
#include "Logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>

int NetworkInterface::setupListener(const std::string &host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG(LogLevel::ERROR, "socket creation failed");
        return -1;
    }

    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!host.empty()) {
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG(LogLevel::ERROR, "bind failed");
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0) {
        LOG(LogLevel::ERROR, "listen failed");
        close(fd);
        return -1;
    }

    if (!setNonBlocking(fd)) {
        close(fd);
        return -1;
    }

    return fd;
}

bool NetworkInterface::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

int NetworkInterface::acceptClient(int listenFd, std::string &clientAddrOut) {
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &len);
    if (clientFd >= 0) {
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        clientAddrOut = addrStr;
        setNonBlocking(clientFd);
    }
    return clientFd;
}

