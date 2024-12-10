#pragma once
#include <string>
#include <cstdint>

class NetworkInterface {
public:
    int setupListener(const std::string &host, int port);
    bool setNonBlocking(int fd);
    int acceptClient(int listenFd, std::string &clientAddrOut);
};
