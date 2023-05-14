#pragma once

#include <fstream>
#include <arpa/inet.h>

#include "device.hpp"
#include "loop/loop.hpp"
#include "loop/sockethandler.hpp"
#include "protocol/message.hpp"

namespace tuya {

class Scanner : public SocketHandler {
public:
    Scanner(Loop& loop, const std::string& devicesFile = "tinytuya/devices.json") : SocketHandler(loop, 6667) {
        std::ifstream ifs(devicesFile);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        mDevices = ordered_json::parse(ifs);

        /* attach to loop as promiscuous handler */
        mLoop.attachExtra(this);
    }

    ~Scanner() {
        mLoop.detachExtra(this);
    }

    ordered_json& knownDevices() {
        return mDevices;
    }

    virtual int handleRead(Event e, const std::string &ip, const ordered_json& data) override {
        (void) data;
        if (e.fd != mSocketFd)
            return SocketHandler::handleRead(e, ip, data);

        /* ignore devices that are already registered */
        if (mConnectedDevices.count(ip))
            return SocketHandler::handleRead(e, ip, data);

        /* register new device */
        auto dev = std::make_unique<Device>(mLoop, ip);
        EV_LOGI(e) << "new device discovered: " << static_cast<std::string>(*dev) << std::endl;
        mConnectedDevices[ip] = std::move(dev);

        return 0;
    }

    virtual int handleClose(CloseEvent& e) override {
        /* cannot erase device while in its callback, need to do it asynchronously */
        mDisconnectedList.push_back(e.addr);
        EV_LOGI(e) << "device " << static_cast<std::string>(*mConnectedDevices[e.addr])
            << " disconnected" << std::endl;

        return SocketHandler::handleClose(e);
    }

    virtual int heartBeat() override {
        for (const auto& ip : mDisconnectedList)
            mConnectedDevices.erase(ip);
        mDisconnectedList.clear();

        return 0;
    }

private:
    virtual const std::string& TAG() override { static const std::string tag = "SCANNER"; return tag; };

    ordered_json mDevices;

    std::map<std::string, std::unique_ptr<Device>> mConnectedDevices;
    std::list<std::string> mDisconnectedList;
};

} // namespace tuya
