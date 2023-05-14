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

    ordered_json& knownDevices() {
        return mDevices;
    }

    virtual int handleRead(Event e, const std::string &ip, const ordered_json& data) override {
        (void) data;
        if (e.fd != mSocketFd)
            return 0;

        /* ignore devices that are already registered */
        if (mConnectedDevices.count(e.fd))
            return 0;

        /* register new device */
        mConnectedDevices[e.fd] = std::make_unique<Device>(mLoop, ip);
        auto& dev = *mConnectedDevices.at(e.fd);

        /* register event callback for closing socket */
        mLoop.attach(dev.fd(), this);

        EV_LOGI(e) << "new device discovered: " << static_cast<std::string>(dev) << std::endl;

        return 0;
    }

    virtual int handleClose(Event e) override {
        if (e.fd == mSocketFd) {
            EV_LOGI(e) << "port is closing" << std::endl;
        } else {
            /* cannot erase device while in its callback, need to do it asynchronously */
            mDisconnectedList.push_back(e.fd);
            EV_LOGI(e) << "device " << static_cast<std::string>(*mConnectedDevices[e.fd])
                << " disconnected" << std::endl;
        }
        return 0;
    }

    virtual int heartBeat() override {
        for (const auto& fd : mDisconnectedList)
            mConnectedDevices.erase(fd);
        mDisconnectedList.clear();

        return 0;
    }

private:
    ordered_json mDevices;

    std::map<int, std::unique_ptr<Device>> mConnectedDevices;
    std::list<int> mDisconnectedList;
};

} // namespace tuya
