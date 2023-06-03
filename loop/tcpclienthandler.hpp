#pragma once

#include <fcntl.h>

#include "sockethandler.hpp"

namespace tuya {

class TCPClientHandler : public SocketHandler {
public:
    TCPClientHandler(Loop& loop, const std::string& ip, int port, const std::string& key)
        : SocketHandler(loop, key, port), mIp(ip) {
        if (inet_pton(mAddr.sin_family, ip.c_str(), &mAddr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address");
        }

        mLoop.pushWork([this] () { connectSocket(); });
    }

    virtual int read(std::string& addr) override {
        addr.assign(mIp);
        return recv(mSocketFd, const_cast<char *>(mBuffer.data()), mBuffer.size(), 0);
    }

    virtual void handleWritable(WritableEvent& e) override {
        int so_error;
        socklen_t len = sizeof(so_error);

        getsockopt(mSocketFd, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
            if (mLoop.attach(mSocketFd, this)) {
                LOGE() << "failed to attach to loop" << std::endl;
            } else {
                mLoop.handleEvent(ConnectedEvent(mSocketFd, mIp, e.logLevel));
            }
        } else {
            EV_LOGW(e) << "failed to connect, retry in " << RECONNECT_DELAY_MS << " ms" << std::endl;
            mLoop.pushWork([this] () { connectSocket(); }, RECONNECT_DELAY_MS);
        }
    }

    virtual void handleConnected(ConnectedEvent& e) override {
        EV_LOGI(e) << "connected to " << mIp << std::endl;
    }

    virtual void handleClose(CloseEvent& e) override {
        EV_LOGI(e) << mIp << " disconnected" << std::endl;
        mLoop.pushWork([this] () { connectSocket(); });
    }

    void connectSocket() {
        int ret = 0;
        mSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mSocketFd < 0) {
            LOGE() << "failed to create socket" << std::endl;
            ret = mSocketFd;
        } else {
            ret = setSocketBlockingEnabled(false);
            if (ret < 0)
                LOGE() << "failed to set socket non-blocking" << std::endl;

        }

        if (ret == 0) {
            ret = mLoop.attachWritable(mSocketFd, this);
            if (ret < 0)
                LOGE() << "failed to attach to loop" << std::endl;

        }

        if (ret == 0) {
            ret = connect(mSocketFd, reinterpret_cast<struct sockaddr *>(&mAddr), sizeof(mAddr));
            if (ret != -1)
                LOGE() << "failed to connecct" << std::endl;
        }

        if (ret != -1) {
            close(mSocketFd);
            mLoop.pushWork([this] () { connectSocket(); }, RECONNECT_DELAY_MS);
        }
    }

private:
    int setSocketBlockingEnabled(bool blocking)
    {
       int flags = fcntl(mSocketFd, F_GETFL, 0);
       if (flags == -1)
           return -1;
       flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
       return fcntl(mSocketFd, F_SETFL, flags);
    }

    const std::string mIp;
};

} // namespace tuya
