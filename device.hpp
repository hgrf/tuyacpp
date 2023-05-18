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
public:
    Device(Loop &loop, const std::string& ip, const std::string& gwId, const std::string& devId, const std::string& key) :
        SocketHandler(loop, ip, 6668, key), mIp(ip), mGwId(gwId), mDevId(devId), mLocalKey(key), mSeqNo(1)
    {
        LOGI() << "connected to " << ip << ": " << (const std::string) *this << std::endl;
        sendCommand(Message::DP_QUERY, ordered_json(), [this](const ordered_json& data) {
            LOGI() << "got response to DP_QUERY: " << data << std::endl;
            mDps = data["dps"];
        });
    }

    Device(Loop &loop, const std::string& ip) :
        // TODO: failure to look up will result in exception -> devices()[ip] should somehow return a default value (maybe device(ip)...)
        Device(loop, ip, devices()[ip]["uuid"], devices()[ip]["id"], devices()[ip]["key"])
    {
    }

    int setOn(bool b) {
        std::string key;
        if (mDps.contains("20"))
            key = "20";
        else if (mDps.contains("1"))
            key = "1";
        else
            return -EINVAL;

        return sendCommand(Message::CONTROL, ordered_json{{key, b}});
    }

    virtual int handleRead(ReadEvent& e) override {
        std::unique_ptr<Message> msg = parse(e.fd, e.data);
        if (!msg->hasData()) {
            EV_LOGE(e) << "failed to parse data in " << static_cast<std::string>(*msg) << std::endl;
            return 0;
        }

        if ((msg->seqNo() == mCmdCtx.seqNo) && (msg->cmd() == static_cast<uint32_t>(mCmdCtx.command))) {
            EV_LOGI(e) << "response to command from " << e.addr << ": " << static_cast<std::string>(*msg) << std::endl;
            mCmdCtx.seqNo = 0;
            if (mCmdCtx.callback != nullptr)
                mCmdCtx.callback(msg->data());
        } else {
            EV_LOGI(e) << "new message from " << e.addr << ": " << static_cast<std::string>(*msg) << std::endl;
        }

        return 0;
    }

    virtual int handleClose(CloseEvent& e) override {
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

    int sendCommand(Message::Command command, const ordered_json& data = ordered_json(), std::function<void(const ordered_json&)> callback = nullptr) {
        if (mCmdCtx.seqNo != 0) {
            LOGE() << "Command already in progress" << std::endl;
            return -EBUSY;
        }

        mCmdCtx.seqNo = mSeqNo++;
        mCmdCtx.command = command;
        mCmdCtx.callback = callback;

        auto payload = ordered_json{
            {"gwId", mDevId}, {"devId", mDevId}, {"uid", mDevId}, {"t", std::to_string((uint32_t) time(NULL))}
        };
        if (command != Message::DP_QUERY)
            payload.erase("gwId");
        if (!data.is_null())
            payload["dps"] = data;
        std::unique_ptr<Message> msg = std::make_unique<Message55AA>(mCmdCtx.seqNo, command, payload);
        LOGI() << "Sending command 0x" << std::hex << command << " with payload: " << payload.dump() << std::endl;
        return sendRaw(msg->serialize(mLocalKey, true));
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
    virtual const std::string& TAG() override { static const std::string tag = "DEVICE"; return tag; };

    struct {
        uint32_t seqNo = 0;
        Message::Command command;
        std::function<void(const ordered_json&)> callback;
    } mCmdCtx;
    std::string mIp;
    std::string mGwId;
    std::string mDevId;
    std::string mLocalKey;
    uint32_t mSeqNo;
    ordered_json mDps;
};

} // namespace tuya
