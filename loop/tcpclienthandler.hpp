#pragma once

#include "sockethandler.hpp"

namespace tuya {

class TCPClientHandler : public SocketHandler {
    const uint32_t RECONNECT_DELAY_MS = 3000;

public:
    TCPClientHandler(Loop& loop, const std::string& ip, int port, const std::string& key) : SocketHandler(loop, key), mIp(ip) {
        mSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mSocketFd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        memset(&mAddr, 0, sizeof(mAddr));
        mAddr.sin_family = AF_INET;
        mAddr.sin_port = htons(port);
        if (inet_pton(mAddr.sin_family, ip.c_str(), &mAddr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address");
        }

        mLoop.pushWork([this] () { connectSocket(); });
    }

    virtual int read(std::string& addr) override {
        addr.assign(mIp);
        return recv(mSocketFd, const_cast<char *>(mBuffer.data()), mBuffer.size(), 0);
    }

    virtual void handleConnected(ConnectedEvent& e) override {
        EV_LOGI(e) << "connected to " << mIp << std::endl;
    }

    void connectSocket() {
        if (connect(mSocketFd, reinterpret_cast<struct sockaddr *>(&mAddr), sizeof(mAddr)) < 0) {
            LOGD() << "failed to connect, retry in " << RECONNECT_DELAY_MS << " ms" << std::endl;
            mLoop.pushWork([this] () { connectSocket(); }, RECONNECT_DELAY_MS);
        } else {
            mLoop.attach(mSocketFd, this);
            mLoop.handleEvent(ConnectedEvent(mSocketFd, LogStream::INFO));
        }
    }

private:
    const std::string mIp;
};

} // namespace tuya
