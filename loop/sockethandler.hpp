#pragma once

#include "loop.hpp"

#include <arpa/inet.h>

#include "../protocol/message55aa.hpp"

namespace tuya {

class SocketHandler : public Handler {
    static const size_t BUFFER_SIZE = 1024;

public:
    SocketHandler(Loop& loop, const std::string& key) : mLoop(loop), mSocketFd(-1), mBuffer("\0", BUFFER_SIZE), mKey(key) {}

    SocketHandler(Loop& loop, int port) : SocketHandler(loop, Message::DEFAULT_KEY) {
        int ret;

        mSocketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (mSocketFd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        
        int broadcast = 1;
        ret = setsockopt(mSocketFd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
        if (ret < 0) {
            throw std::runtime_error("Failed to setsockopt");
        }

        memset(&mAddr, 0, sizeof(mAddr));
        mAddr.sin_family = AF_INET;
        mAddr.sin_addr.s_addr = INADDR_ANY;
        mAddr.sin_port = htons(port);
        ret = bind(mSocketFd, (struct sockaddr *)&mAddr, sizeof(mAddr));
        if (ret < 0) {
            throw std::runtime_error("Failed to bind");
        }

        mLoop.attach(mSocketFd, this);
    };

    virtual int read(std::string& addrStr) {
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

    virtual void handleReadable(ReadableEvent& e) override {
        if ((mSocketFd == -1) || (mSocketFd != e.fd))
            return;

        std::string addr;
        int ret = read(addr);
        if (ret > 0) {
            mLoop.handleEvent(ReadEvent(mSocketFd, mBuffer.substr(0, ret), addr, e.logLevel));
        } else {
            mLoop.pushWork([this, addr, l=e.logLevel] () {
                mLoop.handleEvent(CloseEvent(mSocketFd, addr, l));
                mLoop.detach(mSocketFd);
            });
        }
    }

    virtual void handleRead(ReadEvent& e) override {
        if ((mSocketFd == -1) || (mSocketFd != e.fd))
            return;

        if (e.data.length() < 2 * sizeof(uint32_t))
            throw std::runtime_error("message too short");

        uint32_t prefix = ntohl(*reinterpret_cast<const uint32_t*>(e.data.data()));
        switch(prefix) {
        case Message55AA::PREFIX: {
            Message55AA msg(e.data, mKey, false);
            if (msg.hasData())
                mLoop.handleEvent(MessageEvent(mSocketFd, msg, e.addr, e.logLevel));
            else
                EV_LOGE(e) << "failed to parse data in " << static_cast<std::string>(msg) << std::endl;
            break;
        }
        default:
            EV_LOGE(e) << "unknown prefix: 0x" << std::hex << prefix << std::dec << std::endl;
        }
    }

    virtual void handleClose(CloseEvent& e) override {
        EV_LOGW(e) << "socket closed" << std::endl;
    }

    ~SocketHandler() {
        if (mSocketFd >= 0) {
            mLoop.detach(mSocketFd);
            close(mSocketFd);
        }
    }

    int fd() const { 
        return mSocketFd;
    }

protected:
    Loop& mLoop;
    int mSocketFd;
    struct sockaddr_in mAddr;
    std::string mBuffer;

private:
    std::string mKey;
};

} // namespace tuya
