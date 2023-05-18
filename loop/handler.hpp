#pragma once

#include <list>
#include <string>

#include <arpa/inet.h>

#include "event.hpp"
#include "../logging.hpp"
#include "../protocol/message55aa.hpp"

namespace tuya {

class Handler {
    typedef std::function<int(Event)> EventCallback_t;

public:
    int handle(Event& e) {
        int ret = 0;
        EV_LOGD(e) << "handling " << std::string(e) << std::endl;;

        switch (e.type)
        {
        case Event::READ:
            ret = handleRead(dynamic_cast<ReadEvent&>(e));
            break;
        case Event::CLOSING:
            ret = handleClose(dynamic_cast<CloseEvent&>(e));
            break;
        default:
            break;
        }

        if (ret < 0)
            EV_LOGW(e) << "cb() failed: " << ret << std::endl;

        return ret;
    }

    virtual int handleRead(ReadEvent& e) {
        EV_LOGI(e) << "fd received data" << std::endl;
        return 0;
    }

    virtual int handleClose(CloseEvent& e) {
        EV_LOGI(e) << "fd is closing" << std::endl;
        return 0;
    }

    virtual int heartBeat() {
        return 0;
    }

protected:
    LOG_MEMBERS(HANDLER);
};

} // namespace tuya
