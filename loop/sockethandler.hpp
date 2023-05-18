#pragma once

#include "loop.hpp"

namespace tuya {

class SocketHandler : public Handler {
    const uint32_t RECONNECT_DELAY_MS = 3000;
    static const size_t BUFFER_SIZE = 1024;

public:
    SocketHandler(Loop& loop) : mLoop(loop), mSocketFd(-1), mIp("0.0.0.0"), mKey(Message::DEFAULT_KEY), mBuffer("\0", BUFFER_SIZE) {
        /* register a promiscuous fd handler */
        mLoop.attachExtra(this);
    }

    SocketHandler(Loop& loop, int port) : mLoop(loop), mIp("0.0.0.0"), mKey(Message::DEFAULT_KEY), mBuffer("\0", BUFFER_SIZE) {
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

    SocketHandler(Loop& loop, const std::string& ip, int port, const std::string& key) : mLoop(loop), mIp(ip), mKey(key), mBuffer("\0", BUFFER_SIZE) {
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

    virtual void handleConnected(ConnectedEvent& e) override {
        if ((mSocketFd == -1) || (mSocketFd != e.fd))
            return;

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

    virtual void handleReadable(ReadableEvent& e) override {
        if ((mSocketFd == -1) || (mSocketFd != e.fd))
            return;

        struct sockaddr_in addr;
        unsigned slen = sizeof(sockaddr);

        int ret = recvfrom(e.fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
        if (ret > 0) {
            std::string ip = mIp;
            if (slen) {
                char addr_str[INET_ADDRSTRLEN] = { 0 };
                inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);
                ip = addr_str;
            }
            const std::string& data = mBuffer.substr(0, ret);
            mLoop.handleEvent(ReadEvent(mSocketFd, data, ip, e.logLevel));
        } else {
            mLoop.pushWork([this, l=e.logLevel] () {
                mLoop.handleEvent(CloseEvent(mSocketFd, mIp, l));
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

private:
    struct sockaddr_in mAddr;
    std::string mIp;
    std::string mKey;
    std::string mBuffer;
};

} // namespace tuya
