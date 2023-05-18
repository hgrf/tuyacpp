#pragma once

#include "loop.hpp"

namespace tuya {

class SocketHandler : public Handler {
    static const size_t BUFFER_SIZE = 1024;

public:
    SocketHandler(Loop& loop, const std::string& key) : mLoop(loop), mBuffer("\0", BUFFER_SIZE), mKey(key) {}

    SocketHandler(Loop& loop) : SocketHandler(loop, Message::DEFAULT_KEY) {
        /* register a promiscuous fd handler */
        mLoop.attachExtra(this);
    }

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

    virtual std::unique_ptr<Message> parse(int fd, const std::string& data) {
        /* use the parser from the SocketHandler that specifically belongs to this FD
         * so that the correct key is used for message decrypting
         */
        if (fd != mSocketFd) {
            Handler* handler = mLoop.getHandler(fd);
            SocketHandler* socketHandler = dynamic_cast<SocketHandler*>(handler);
            if (socketHandler != nullptr)
                return socketHandler->parse(fd, data);
            else
                return std::make_unique<Message55AA>(data, mKey, false);
        }

        if (data.length() < 2 * sizeof(uint32_t))
            throw std::runtime_error("message too short");

        uint32_t prefix = ntohl(*reinterpret_cast<const uint32_t*>(data.data()));
        switch(prefix) {
        case Message55AA::PREFIX:
            return std::make_unique<Message55AA>(data, mKey, false);
            break;
        default:
            throw std::runtime_error("unknown prefix");
        }
    }

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
        EV_LOGI(e) << "socket received " << e.data.size() << " bytes" << std::endl;
    }

    virtual void handleClose(CloseEvent& e) override {
        EV_LOGW(e) << "socket closed" << std::endl;
    }

    ~SocketHandler() {
        if (mSocketFd == -1) {
            mLoop.detachExtra(this);
        } else {
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
