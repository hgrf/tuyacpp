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
    virtual const std::string& TAG() override { static const std::string tag = "SCANNER"; return tag; };

    Scanner(Loop& loop, const std::string& devicesFile = "tinytuya/devices.json") : SocketHandler(loop, 6667) {
        std::ifstream ifs(devicesFile);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        mDevices = ordered_json::parse(ifs);
    }

    virtual int handleRead(Event e, const std::string &ip, const ordered_json& data) override {
        (void) data;

        /* ignore devices that are already registered */
        if (mConnectedDevices.count(ip))
            return 0;

        /* register new device */
        mConnectedDevices[ip] = std::make_unique<Device>(mLoop, ip);
        auto& dev = *mConnectedDevices.at(ip);

        /* register event callback for closing socket */
        dev.registerEventCallback(Event::CLOSING, [this, ip](Event e) {
            EV_LOGI(e) << "device " << ip << " disconnected" << std::endl;
            /* cannot erase device while in its callback, need to do it asynchronously */
            mDisconnectedList.push_back(ip);
            return 0;
        });

        EV_LOGI(e) << "new device discovered: " << static_cast<std::string>(dev) << std::endl;

        return 0;
    }

    virtual int handleClose(Event e) override {
        EV_LOGI(e) << "port is closing" << std::endl;

        return 0;
    }

    virtual int heartBeat() override {
        for (const auto& ip : mDisconnectedList)
            mConnectedDevices.erase(ip);
        mDisconnectedList.clear();

        return 0;
    }

private:
    ordered_json mDevices;

    std::map<std::string, std::unique_ptr<Device>> mConnectedDevices;
    std::list<std::string> mDisconnectedList;
};

} // namespace tuya
