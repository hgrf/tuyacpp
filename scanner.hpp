#pragma once

#include <fstream>
#include <iostream>
#include <arpa/inet.h>

#include "device.hpp"
#include "protocol/message.hpp"

namespace tuya {

class Scanner {
public:
    Scanner(const std::string& devicesFile = "tinytuya/devices.json") {
        int ret;

        std::ifstream ifs(devicesFile);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        mDevices = json::parse(ifs);
        
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
    }

    void scan() {
        struct sockaddr_in addr;
        for (;;) {
            char buffer[1024];
            unsigned slen=sizeof(sockaddr);
            int ret = recvfrom(mSocketFd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &slen);
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);
            if (ret > 0) {
                std::cout << "Received " << ret << " bytes from " << addr_str << std::endl;
                auto msg = parse((unsigned char*) buffer, ret);
                // Message msg(reinterpret_cast<uint8_t *>(buffer), ret);
                std::cout << "Received message: " << static_cast<std::string>(*msg) << std::endl;

                for (const auto& device : mDevices) {
                    if (device["ip"] == msg->data()["ip"]) {
                        std::cout << "Known device:" << device << std::endl;
                    }
                }
                // TODO: better flow control
                return;

                // for (const auto& device : devices) {
                //     if (device["ip"] == mData["ip"]) {
                //         mData["name"] = device["name"];
                //         mData["devId"] = device["id"];
                //         mData["localKey"] = device["key"];
                //         break;
                //     }
                // }

                // // TODO: uid?
                // TuyaDevice(
                //     mData["ip"].get<std::string>(), mData["gwId"].get<std::string>(),
                //     mData["devId"].get<std::string>(), "", mData["localKey"].get<std::string>()
                // );
            }
        }
    }

private:
    int mSocketFd;
    ordered_json mDevices;
};

} // namespace tuya
