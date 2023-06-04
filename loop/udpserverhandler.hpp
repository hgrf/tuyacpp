#pragma once

#include "sockethandler.hpp"

namespace tuya {

class UDPServerHandler : public SocketHandler {
public:
    UDPServerHandler(Loop& loop, int port, bool attachToLoop)
        : SocketHandler(loop, Message::DEFAULT_KEY, port), mAttachToLoop(attachToLoop) {
        mAddr.sin_addr.s_addr = INADDR_ANY;

        mLoop.pushWork([this] () { bindSocket(); });
    };

    void bindSocket() {
        int ret;
        int broadcast = 1;
        mSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (mSocketFd < 0) {
            LOGE() << "failed to create socket" << std::endl;
            ret = mSocketFd;
        } else {
            ret = setsockopt(mSocketFd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
            if (ret < 0)
                LOGE() << "failed to setsockopt" << std::endl;
        }

        if (ret >= 0) {
            ret = bind(mSocketFd, (struct sockaddr *)&mAddr, sizeof(mAddr));
            if (ret < 0)
                LOGE() << "failed to bind" << std::endl;
        }

        if (ret >= 0) {
            if (mAttachToLoop)
                mLoop.attach(mSocketFd, this);
        } else {
            mLoop.pushWork([this] () { bindSocket(); }, RECONNECT_DELAY_MS);
        }
    }

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

private:
    bool mAttachToLoop;
};

} // namespace tuya
