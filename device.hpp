#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <unordered_map>
#include <string>

#include <arpa/inet.h>
#include <unistd.h>

#include "loop.hpp"
#include "protocol/protocol.hpp"
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;


namespace tuya {

class Device {
    enum Command {
        DP_QUERY        = 0x0a,  //  10 // FRM_QUERY_STAT      // UPDATE_START_CMD - get data points
    };

public:
    Device(Loop &loop, const std::string& ip, const std::string& gwId, const std::string& devId, const std::string& key) :
        mGwId(gwId), mDevId(devId), mLocalKey(key), mLoop(loop), mLoopHandler(key)
    {
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

        mLoop.attach(mSocketFd, &mLoopHandler);
        sendCommand(DP_QUERY);
    }

    Device(Loop &loop, const std::string& ip) :
        // TODO: failure to look up will result in exception -> devices()[ip] should somehow return a default value (maybe device(ip)...)
        Device(loop, ip, devices()[ip]["uuid"], devices()[ip]["id"], devices()[ip]["key"])
    {
    }

    ~Device() {
        mLoop.detach(mSocketFd);
        close(mSocketFd);
    }

    int sendRaw(const std::string& message) {
        int ret = send(mSocketFd, message.data(), message.length(), 0);
        if (ret <= 0) {
            std::cerr << "Failed to send message" << std::endl;
            return -1;
        } else if ((unsigned) ret != message.length()) {
            std::cerr << "Failed to send message" << std::endl;
            return -1;
        }

        std::cout << "Message sent, waiting for response..." << std::endl;

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

    operator std::string() const {
        std::ostringstream ss;
        ss << std::hex << "Device { gwId: " << mGwId << ", devId: " << mDevId
            << ", localKey: " << mLocalKey << " }";
        return ss.str();
    }

    static ordered_json& devices() {
        if (!mDevices.size()) {
            const std::string devicesFile = "tinytuya/devices.json";
            std::ifstream ifs(devicesFile);
            if (!ifs.is_open()) {
                throw std::runtime_error("Failed to open file");
            }
            auto devices = ordered_json::parse(ifs);
            for (const auto& dev : devices)
                mDevices[dev.at("ip")] = dev;
        }

        return mDevices;
    }

private:
    class LoopHandler : public Loop::Handler {
    public:
        LoopHandler(const std::string& key) : Loop::Handler(key) {}

        virtual int handle(int fd, Loop::Event e, bool verbose) override {
            (void) verbose;
            int ret = Loop::Handler::handle(fd, e, false);
            if ((ret < 0) || Loop::Event::Type(e) != Loop::Event::READ)
                return ret;

            std::cout << "[DEVICE] new message from device: " << static_cast<std::string>(*mMsg) << std::endl;
        }
    };

    int mSocketFd;
    std::string mGwId;
    std::string mDevId;
    std::string mLocalKey;
    Loop& mLoop;
    LoopHandler mLoopHandler;
    static ordered_json mDevices;
};

} // namespace tuya

#ifdef TUYA_SINGLE_HEADER
#include "device.cpp"
#endif
