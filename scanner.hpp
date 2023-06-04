#pragma once

#include <fstream>
#include <arpa/inet.h>

#include "device.hpp"
#include "loop/udpserverhandler.hpp"
#include "protocol/message.hpp"

namespace tuya {

class Scanner : public UDPServerHandler {
public:
    Scanner(Loop& loop, const std::string& devicesFile = "tinytuya/devices.json") : UDPServerHandler(loop, 6667) {
        std::ifstream ifs(devicesFile);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        mKnownDevices = ordered_json::parse(ifs);

        /* attach to loop as promiscuous handler */
        mLoop.attach(this);

        /* register all known devices */
        for (const auto& devDesc : mKnownDevices) {
            const auto& addr = devDesc["ip"];
            mDevices[addr] = std::make_shared<Device>(mLoop, addr);
        }
    }

    ~Scanner() {
        mLoop.detach(this);
    }

    std::set<std::string> getDevices() {
        std::set<std::string> devices;
        for (const auto& it : mDevices)
            devices.insert(it.first);
        return devices;
    }

    const ordered_json& knownDevices() {
        return mKnownDevices;
    }

    std::shared_ptr<Device> getDevice(const std::string& ip) {
        if (mDevices.count(ip))
            return mDevices.at(ip);
        return std::shared_ptr<Device>();
    }

    virtual void handleMessage(MessageEvent& e) override {
        if (e.fd != mSocketFd)
            return;

        /* ignore devices that are already registered */
        if (mDevices.count(e.addr)) {
            EV_LOGD(e) << "ignoring known device " << e.addr << std::endl;
            return;
        }

        /* register new device */
        auto dev = std::make_shared<Device>(mLoop, e.addr);
        EV_LOGI(e) << "new device discovered: " << static_cast<std::string>(*dev) << std::endl;
        mDevices[e.addr] = std::move(dev);
    }

    virtual void handleClose(CloseEvent& e) override {
        EV_LOGI(e) << static_cast<std::string>(*mDevices[e.addr])
            << " disconnected" << std::endl;
    }

private:
    virtual const std::string& TAG() override { static const std::string tag = "SCANNER"; return tag; };

    ordered_json mKnownDevices;

    std::map<std::string, std::shared_ptr<Device>> mDevices;
};

} // namespace tuya
