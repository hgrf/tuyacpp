#pragma once

#include "loop.hpp"

namespace tuya {

class SocketHandler : public Handler {
public:
    SocketHandler(Loop& loop) : mLoop(loop), mSocketFd(-1), mKey(Message::DEFAULT_KEY) {
        /* register a promiscuous fd handler */
        mLoop.attachExtra(this);
    }

    SocketHandler(Loop& loop, int port) : mLoop(loop), mKey(Message::DEFAULT_KEY) {
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

        struct sockaddr_in myaddr;
        memset(&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = INADDR_ANY;
        myaddr.sin_port = htons(port);
        ret = bind(mSocketFd, (struct sockaddr *)&myaddr, sizeof(myaddr));
        if (ret < 0) {
            throw std::runtime_error("Failed to bind");
        }

        mLoop.attach(mSocketFd, this);
    };

    SocketHandler(Loop& loop, const std::string& ip, int port, const std::string& key) : mLoop(loop), mKey(key) {
        mSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mSocketFd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address");
        }

        if (connect(mSocketFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            throw std::runtime_error("Failed to connect");
        }

        mLoop.attach(mSocketFd, ip, this);
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

    virtual int handleRead(Event e, const std::string& ip, const ordered_json& data) {
        EV_LOGI(e) << "received message from " << ip << ": " << data << std::endl;
        return 0;
    }

    virtual int handleRead(ReadEvent& e) override {
        EV_LOGD(e) << "parsing " << e.data.size() << " bytes" << std::endl;
        mMsg = parse(e.fd, e.data);
        return mMsg->hasData() ? handleRead(e, e.addr, mMsg->data()) : 0;
    }

    virtual int handleClose(CloseEvent& e) override {
        if ((mSocketFd != -1) && (e.fd == mSocketFd)) {
            EV_LOGW(e) << "socket closed" << std::endl;
            return -1; // tell the loop to detach this handler
        }
        return 0;
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
    std::string mKey;
};

} // namespace tuya
