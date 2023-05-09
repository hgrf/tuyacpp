#pragma once

#include <fstream>
#include <arpa/inet.h>

#include "device.hpp"
#include "loop.hpp"
#include "protocol/message.hpp"

namespace tuya {

class Scanner : public Loop::Handler {
    const std::string TAG = "SCANNER";

public:
    Scanner(Loop& loop, const std::string& devicesFile = "tinytuya/devices.json") : mLoop(loop) {
        int ret;

        std::ifstream ifs(devicesFile);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        mDevices = ordered_json::parse(ifs);
        
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
        myaddr.sin_port = htons(6667);
        ret = bind(mSocketFd, (struct sockaddr *)&myaddr, sizeof(myaddr));
        if (ret < 0) {
            throw std::runtime_error("Failed to bind");
        }

        mLoop.attach(mSocketFd, this);
    }

    ~Scanner() {
        mLoop.detach(mSocketFd);
        close(mSocketFd);
    }

    virtual int handleRead(Loop::Event e, const std::string &ip, const ordered_json& data) override {
        (void) data;

        /* ignore devices that are already registered */
        if (mConnectedDevices.count(ip))
            return 0;

        /* register new device */
        mConnectedDevices[ip] = std::make_unique<Device>(mLoop, ip);
        auto& dev = *mConnectedDevices.at(ip);

        /* register event callback for closing socket */
        dev.registerEventCallback(Loop::Event::CLOSING, [this, ip](Loop::Event e) {
            EV_LOG(e) << "device " << ip << " disconnected" << std::endl;
            /* cannot erase device while in its callback, need to do it asynchronously */
            mDisconnectedList.push_back(ip);
            return 0;
        });

        EV_LOG(e) << "new device discovered: " << static_cast<std::string>(dev) << std::endl;

        return 0;
    }

    virtual int handleClose(Loop::Event e) override {
        EV_LOG(e) << "port is closing" << std::endl;

        return 0;
    }

    virtual int heartBeat() override {
        for (const auto& ip : mDisconnectedList)
            mConnectedDevices.erase(ip);
        mDisconnectedList.clear();

        return 0;
    }

private:
    Loop& mLoop;
    int mSocketFd;
    ordered_json mDevices;

    std::map<std::string, std::unique_ptr<Device>> mConnectedDevices;
    std::list<std::string> mDisconnectedList;
};

} // namespace tuya
