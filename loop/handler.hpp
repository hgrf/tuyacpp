#pragma once

#include <string>

#include "event.hpp"
#include "../logging.hpp"

namespace tuya {

class Handler {
    typedef std::function<int(Event)> EventCallback_t;

public:
    void handle(Event& e) {
        EV_LOGD(e) << "handling " << std::string(e) << std::endl;;

        switch (e.type)
        {
        case Event::CONNECTED:
            handleConnected(dynamic_cast<ConnectedEvent&>(e));
            break;
        case Event::READABLE:
            handleReadable(dynamic_cast<ReadableEvent&>(e));
            break;
        case Event::WRITABLE:
            handleWritable(dynamic_cast<WritableEvent&>(e));
            break;
        case Event::READ:
            handleRead(dynamic_cast<ReadEvent&>(e));
            break;
        case Event::MESSAGE:
            handleMessage(dynamic_cast<MessageEvent&>(e));
            break;
        case Event::CLOSING:
            handleClose(dynamic_cast<CloseEvent&>(e));
            break;
        default:
            break;
        }
    }

    virtual void handleConnected(ConnectedEvent& e) {
        EV_LOGD(e) << "fd is connected" << std::endl;
    }

    virtual void handleReadable(ReadableEvent& e) {
        EV_LOGD(e) << "fd is readable" << std::endl;
    }

    virtual void handleWritable(WritableEvent& e) {
        EV_LOGD(e) << "fd is writable" << std::endl;
    }

    virtual void handleRead(ReadEvent& e) {
        EV_LOGD(e) << "fd received data" << std::endl;
    }

    virtual void handleMessage(MessageEvent& e) {
        EV_LOGD(e) << "fd received message" << std::endl;
    }

    virtual void handleClose(CloseEvent& e) {
        EV_LOGD(e) << "fd is closing" << std::endl;
    }

protected:
    LOG_MEMBERS(HANDLER);
    std::ostream& EV_LOGD(Event &e) { return e.log(TAG(), LogStream::DEBUG); }
    std::ostream& EV_LOGI(Event &e) { return e.log(TAG(), LogStream::INFO); }
    std::ostream& EV_LOGW(Event &e) { return e.log(TAG(), LogStream::WARNING); }
    std::ostream& EV_LOGE(Event &e) { return e.log(TAG(), LogStream::ERROR); }
};

} // namespace tuya
