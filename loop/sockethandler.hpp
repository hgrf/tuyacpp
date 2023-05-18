#pragma once

#include "loop.hpp"

#include <arpa/inet.h>

#include "../protocol/message55aa.hpp"

namespace tuya {

class SocketHandler : public Handler {
    static const size_t BUFFER_SIZE = 1024;

public:
    SocketHandler(Loop& loop, const std::string& key, int port)
        : mLoop(loop), mSocketFd(-1), mBuffer("\0", BUFFER_SIZE), mKey(key) {
        memset(&mAddr, 0, sizeof(mAddr));
        mAddr.sin_family = AF_INET;
        mAddr.sin_port = htons(port);
    }

    virtual int read(std::string& addrStr) = 0;

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
