#pragma once

#include "sockethandler.hpp"

namespace tuya {

class UDPServerHandler : public SocketHandler {
public:
    UDPServerHandler(Loop& loop, int port)
        : SocketHandler(loop, Message::DEFAULT_KEY, port) {
        int ret;
        int broadcast = 1;
        mAddr.sin_addr.s_addr = INADDR_ANY;

        mSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (mSocketFd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        
        ret = setsockopt(mSocketFd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
        if (ret < 0) {
            throw std::runtime_error("Failed to setsockopt");
        }

        ret = bind(mSocketFd, (struct sockaddr *)&mAddr, sizeof(mAddr));
        if (ret < 0) {
            throw std::runtime_error("Failed to bind");
        }

        mLoop.attach(mSocketFd, this);
    };

    virtual int read(std::string& addrStr) override {
        struct sockaddr_in addr;
        unsigned slen = sizeof(sockaddr);
        int ret = recvfrom(mSocketFd, const_cast<char*>(mBuffer.data()), mBuffer.size(), 0, (struct sockaddr *)&addr, &slen);
        if (ret > 0 && slen) {
            char addr_str[INET_ADDRSTRLEN] = { 0 };
            inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);
            addrStr.assign(addr_str);
        }
        return ret;
    }
};

} // namespace tuya