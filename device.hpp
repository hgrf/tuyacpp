#pragma once

#include <fstream>
#include <functional>
#include <map>
#include <unordered_map>
#include <string>

#include <arpa/inet.h>
#include <unistd.h>

#include "loop/loop.hpp"
#include "loop/sockethandler.hpp"
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;


namespace tuya {

class Device : public SocketHandler {
    enum Command {
        DP_QUERY        = 0x0a,  //  10 // FRM_QUERY_STAT      // UPDATE_START_CMD - get data points
    };

public:
    virtual const std::string& TAG() override { static const std::string tag = "DEVICE"; return tag; };

    Device(Loop &loop, const std::string& ip, const std::string& gwId, const std::string& devId, const std::string& key) :
        SocketHandler(loop, ip, 6668, key), mIp(ip), mGwId(gwId), mDevId(devId), mLocalKey(key)
    {
        LOGI() << "connected to " << ip << ": " << (const std::string) *this << std::endl;
        sendCommand(DP_QUERY);
    }

    Device(Loop &loop, const std::string& ip) :
        // TODO: failure to look up will result in exception -> devices()[ip] should somehow return a default value (maybe device(ip)...)
        Device(loop, ip, devices()[ip]["uuid"], devices()[ip]["id"], devices()[ip]["key"])
    {
    }

    virtual int handleRead(Event e, const std::string& ip, const ordered_json& data) override {
        EV_LOGI(e) << "new message from " << ip << ": " << data << std::endl;
        return 0;
    }

    virtual int handleClose(Event e) override {
        EV_LOGI(e) << mIp << " disconnected" << std::endl;
        return 0;
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

        LOGI() << "sent " << ret << " bytes to " << mIp << std::endl;

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

    const std::string& ip() const {
        return mIp;
    }

    operator std::string() const {
        std::ostringstream ss;
        ss << std::hex << "Device { gwId: " << mGwId << ", devId: " << mDevId
            << ", localKey: " << mLocalKey << " }";
        return ss.str();
    }

    static ordered_json& devices() {
        static ordered_json sDevices = ordered_json{};

        if (!sDevices.size()) {
            const std::string devicesFile = "tinytuya/devices.json";
            std::ifstream ifs(devicesFile);
            if (!ifs.is_open()) {
                throw std::runtime_error("Failed to open file");
            }
            auto devices = ordered_json::parse(ifs);
            for (const auto& dev : devices)
                sDevices[dev.at("ip")] = dev;
        }

        return sDevices;
    }

private:
    std::string mIp;
    std::string mGwId;
    std::string mDevId;
    std::string mLocalKey;
};

} // namespace tuya
