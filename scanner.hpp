#pragma once

#include <fstream>
#include <iostream>
#include <arpa/inet.h>

#include "device.hpp"
#include "loop.hpp"
#include "protocol/message.hpp"

namespace tuya {

class Scanner {
public:
    Scanner(Loop& loop, const std::string& devicesFile = "tinytuya/devices.json") : mLoop(loop), mLoopHandler(loop) {
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

        mLoop.attach(mSocketFd, &mLoopHandler);
    }

    ~Scanner() {
        mLoop.detach(mSocketFd);
        close(mSocketFd);
    }

private:
    class LoopHandler : public Loop::Handler {
    public:
        LoopHandler(Loop& loop) : mLoop(loop) {}

        virtual int handle(int fd, Loop::Event e, bool verbose) override {
            (void) verbose;
            int ret = Loop::Handler::handle(fd, e, false);
            if ((ret < 0) || Loop::Event::Type(e) != Loop::Event::READ)
                return ret;

            const std::string ip = mMsg->data()["ip"];
            if (mDevices.count(ip))
                return ret;

            mDevices[ip] = std::make_unique<Device>(mLoop, ip);
            std::cout << "[SCANNER] new device discovered: " << static_cast<std::string>(*mDevices.at(ip)) << std::endl;
        }
    private:
        Loop& mLoop;
        std::map<std::string, std::unique_ptr<Device>> mDevices;
    };

    Loop& mLoop;
    LoopHandler mLoopHandler;
    int mSocketFd;
    ordered_json mDevices;
};

} // namespace tuya
