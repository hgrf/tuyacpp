#pragma once

#include <fstream>
#include <functional>
#include <string>

#include "loop/tcpclienthandler.hpp"
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;


namespace tuya {

class Device : public TCPClientHandler {
public:
    enum CommandStatus {
        CMD_OK,
        CMD_ERR_DISCONNECTED,
    };

    typedef std::function<void(CommandStatus, const ordered_json&)> Callback_t;

    Device(Loop &loop, const std::string& ip, const std::string& name, const std::string& gwId, const std::string& devId, const std::string& key) :
        TCPClientHandler(loop, ip, 6668, key), mTag("DEVICE " + ip), mIp(ip), mName(name), mGwId(gwId), mDevId(devId), mLocalKey(key), mSeqNo(1)
    {
    }

    bool isOn() {
        const auto& key = switchKey();
        if (key.length())
            return mDps[key];
        return false;
    }

    int setOn(bool b, Callback_t cb = nullptr) {
        const auto& key = switchKey();
        if (key.length())
            return sendCommand(Message::CONTROL, ordered_json{{key, b}}, cb);
        return -EINVAL;
    }

    int toggle(Callback_t cb = nullptr) {
        const auto& key = switchKey();
        if (key.length())
            return sendCommand(Message::CONTROL, ordered_json{{key, !mDps[key]}}, cb);
        return -EINVAL;
    }

    int setBrightness(int brightness, Callback_t cb = nullptr) {
        const auto& key = brightnessKey();
        if (key == "2")
            brightness = std::max(brightness, 25);
        if (key.length())
            return sendCommand(Message::CONTROL, ordered_json{{key, brightness}}, cb);
        return -EINVAL;
    }

    int setColorTemp(int colorTemp, Callback_t cb = nullptr) {
        const auto& key = colorTempKey();
        if (key.length())
            return sendCommand(Message::CONTROL, ordered_json{{key, colorTemp}}, cb);
        return -EINVAL;
    }

    virtual void handleMessage(MessageEvent& e) override {
        const auto& msg = e.msg;
        const auto& msgStr = static_cast<std::string>(msg);
        if ((msg.seqNo() == mCmdCtx.seqNo) && (msg.cmd() == static_cast<uint32_t>(mCmdCtx.command))) {
            EV_LOGI(e) << "response to command " << msg.cmdString() << " from " << e.addr << ": " << msgStr << std::endl;
            mCmdCtx.seqNo = 0;
            if (mCmdCtx.callback != nullptr)
                mCmdCtx.callback(CMD_OK, msg.data());
        } else if (msg.cmd() == Message::STATUS) {
            mDps.update(msg.data()["dps"]);
        } else {
            EV_LOGI(e) << "new message from " << e.addr << ": " << msgStr << std::endl;
        }
    }

    virtual void handleConnected(ConnectedEvent& e) override {
        TCPClientHandler::handleConnected(e);

        sendCommand(Message::DP_QUERY, ordered_json(), [this](CommandStatus status, const ordered_json& data) {
            if (status == CMD_OK) {
                mDps = data["dps"];
            } else {
                LOGE() << "command failed, error " << status << std::endl;
            }
        });
    }

    virtual void handleClose(CloseEvent& e) override {
        TCPClientHandler::handleClose(e);

        if (mCmdCtx.seqNo) {
            mCmdCtx.seqNo = 0;
            if (mCmdCtx.callback != nullptr)
                mCmdCtx.callback(CMD_ERR_DISCONNECTED, ordered_json());
        }
    }

    /* sendRaw() can be called from outside the loop thread because only read operations
     * are performed in the loop thread and socket read and write operations are
     * independent
     */
    int sendRaw(const std::string& message) {
        if (!isConnected()) {
            LOGE() << "failed to send message: not connected" << std::endl;
            return -ENOTCONN;
        }

        int ret = send(mSocketFd, message.data(), message.length(), 0);
        if (ret <= 0) {
            LOGE() << "failed to send message" << std::endl;
            return -1;
        } else if ((unsigned) ret != message.length()) {
            LOGE() << "failed to send message" << std::endl;
            return -1;
        }

        LOGI() << "sent " << ret << " bytes to " << mIp << std::endl;

        return 0;
    }

    int sendCommand(Message::Command command, const ordered_json& data = ordered_json(), Callback_t callback = nullptr) {
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
        LOGI() << "sending command " << msg->cmdString() << " with payload: " << payload.dump() << std::endl;
        mLoop.pushWork([this, seqno=mCmdCtx.seqNo] () {
            if (mCmdCtx.seqNo == seqno) {
                LOGE() << "timeout" << std::endl;
                mLoop.handleEvent(CloseEvent(mSocketFd, mIp, LogStream::INFO));
            }
        }, 3000);
        return sendRaw(msg->serialize(mLocalKey, true));
    }

    const std::string& ip() const {
        return mIp;
    }

    const std::string& name() const {
        return mName;
    }

    operator std::string() const {
        std::ostringstream ss;
        ss << "Device { name: " << mName << " gwId: " << mGwId << ", devId: " << mDevId
            << ", localKey: " << mLocalKey << " }";
        return ss.str();
    }

    int brightnessScale() {
        if (mDps.contains("22"))
            return 1000;
        else if (mDps.contains("2"))
            return 255;
        return 1;
    }

    int colorTempScale() {
        if (mDps.contains("23"))
            return 1000;
        else if (mDps.contains("3"))
            return 255;
        return 1;
    }

private:
    virtual const std::string& TAG() override { return mTag; };

    std::string switchKey() {
        if (mDps.contains("20"))
            return "20";
        else if (mDps.contains("1"))
            return "1";
        return "";
    }

    std::string brightnessKey() {
        if (mDps.contains("22"))
            return "22";
        else if (mDps.contains("2"))
            return "2";
        return "";
    }

    std::string colorTempKey() {
        if (mDps.contains("23"))
            return "23";
        else if (mDps.contains("3"))
            return "3";
        return "";
    }

    struct {
        uint32_t seqNo = 0;
        Message::Command command;
        Callback_t callback;
    } mCmdCtx;
    const std::string mTag;
    const std::string mIp;
    const std::string mName;
    const std::string mGwId;
    const std::string mDevId;
    const std::string mLocalKey;
    uint32_t mSeqNo;
    ordered_json mDps;
};

} // namespace tuya
