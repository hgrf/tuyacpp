#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <string>

#include <arpa/inet.h>
#include <unistd.h>

#include "protocol/protocol.hpp"
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;


namespace tuya {

class Device {
    enum Command {
        DP_QUERY        = 0x0a,  //  10 // FRM_QUERY_STAT      // UPDATE_START_CMD - get data points
    };

public:
    Device(const std::string& ip) {
        for (const auto& device : devices()) {
            if (device["ip"] == ip) {
                mGwId = device["uuid"];
                mDevId = device["id"];
                mLocalKey = device["key"];
            }
        }

        mSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mSocketFd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(6668);
        if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address");
        }

        if (connect(mSocketFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            throw std::runtime_error("Failed to connect");
        }

        std::cout << "Connected to " << ip << ": " << (const std::string) *this << std::endl;

        sendCommand(DP_QUERY);
    }

    ~Device() {
        if (mSocketFd >= 0)
            close(mSocketFd);
    }

    int sendRaw(const std::string& message) {
        if (send(mSocketFd, message.data(), message.length(), 0) != message.length()) {
            std::cerr << "Failed to send message" << std::endl;
            return -1;
        }

        std::cout << "Message sent, waiting for response..." << std::endl;

        char recvBuffer[512];
        ssize_t recvLen = recv(mSocketFd, recvBuffer, sizeof(recvBuffer), 0);
        if (recvLen > 0) {
            std::cout << "Received " << recvLen << " bytes" << std::endl;
            auto resp = Message::deserialize(std::string(recvBuffer, recvLen), mLocalKey);
            std::cout << "Received message: " << static_cast<std::string>(*resp) << std::endl;
        } else {
            std::cerr << "recv() returned " << recvLen << std::endl;
        }

        return 0;
    }

    int sendCommand(Command command) {
        auto payload = ordered_json{
            {"gwId", mDevId}, {"devId", mDevId}, {"uid", mDevId}, {"t", std::to_string((uint32_t) time(NULL))}
        };
        // TODO
        uint32_t seqNo = 1;
        std::unique_ptr<Message> msg = std::make_unique<Message55AA>(seqNo, command, payload);
        return sendRaw(msg->serialize(mLocalKey));
    }

    operator const std::string() const {
        std::ostringstream ss;
        ss << std::hex << "Tuya Device { gwId: " << mGwId << ", devId: " << mDevId
            << ", localKey: " << mLocalKey << " }";
        return ss.str();
    }

    static const ordered_json& devices() {
        if (!mDevices.size()) {
            const std::string devicesFile = "tinytuya/devices.json";
            std::ifstream ifs(devicesFile);
            if (!ifs.is_open()) {
                throw std::runtime_error("Failed to open file");
            }
            mDevices = ordered_json::parse(ifs);
        }

        return mDevices;
    }

private:
    int mSocketFd;
    std::string mGwId;
    std::string mDevId;
    std::string mLocalKey;
    static ordered_json mDevices;
};

} // namespace tuya

#ifdef TUYA_SINGLE_HEADER
#include "device.cpp"
#endif
