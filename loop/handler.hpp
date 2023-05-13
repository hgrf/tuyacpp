#pragma once

#include <list>
#include <string>

#include <arpa/inet.h>

#include "event.hpp"
#include "../logging.hpp"
#include "../protocol/message.hpp"

namespace tuya {

class Handler {
    typedef std::function<int(Event)> EventCallback_t;

    static const size_t BUFFER_SIZE = 1024;

public:
    LogStream& LOGGER(LogStream::Level lev) { return LogStream::get(TAG(), lev); }
    virtual const std::string& TAG() { static const std::string tag = "HANDLER"; return tag; };

    Handler(const std::string& key = DEFAULT_KEY) : mBuffer("\0", BUFFER_SIZE), mKey(key) {
        registerEventCallback(Event::READ, [this](Event e) {
            struct sockaddr_in addr;
            unsigned slen=sizeof(sockaddr);
            int ret = recvfrom(e.fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
            if (ret <= 0)
                return -EINVAL;

            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

            mMsg = Message::deserialize(mBuffer.substr(0, ret), mKey);

            return handleRead(e, addr_str, mMsg->data());
        });

        registerEventCallback(Event::CLOSING, [this](Event e) {
            return handleClose(e);
        });
    }

    void registerEventCallback(Event::Type t, EventCallback_t cb) {
        const auto& it = mEventCallbacks.find(t);
        if (it == mEventCallbacks.end())
            mEventCallbacks[t] = {cb};
        else
            /* event callbacks are processed in the inverse order of their
                * registering order, so that - in particular for the CLOSING event
                * - the socket is closed after all other callbacks have been processed
                */
            mEventCallbacks.at(t).push_front(cb);
    }

    int handle(Event e) {
        int ret = 0;
        EV_LOGD(e) << "handling " << std::string(e) << std::endl;;

        const auto& it = mEventCallbacks.find(e.type);
        if (it == mEventCallbacks.end())
            return 0;

        for (const auto &cb : it->second) {
            ret = cb(e);
            if (ret < 0) {
                EV_LOGW(e) << "cb() failed: " << ret << std::endl;
                return ret;
            }
        }

        return 0;
    }

    virtual int handleRead(Event e, const std::string& ip, const ordered_json& data) {
        EV_LOGI(e) << "received message from " << ip << ": " << data << std::endl;
        return 0;
    }

    virtual int handleClose(Event e) {
        EV_LOGI(e) << "socket is closing" << std::endl;
        return 0;
    }

    virtual int heartBeat() {
        return 0;
    }

protected:
    LogStream& LOGD() { return LOGGER(LogStream::DEBUG); }
    LogStream& LOGI() { return LOGGER(LogStream::INFO); }
    LogStream& LOGW() { return LOGGER(LogStream::WARNING); }
    LogStream& LOGE() { return LOGGER(LogStream::ERROR); }
    std::ostream& EV_LOG(Event &e, LogStream::Level l) {
        return e.log(LOGGER(l));
    }
    std::ostream& EV_LOGD(Event &e) {
        return EV_LOG(e, LogStream::DEBUG);
    }
    std::ostream& EV_LOGI(Event &e) {
        return EV_LOG(e, LogStream::INFO);
    }
    std::ostream& EV_LOGW(Event &e) {
        return EV_LOG(e, LogStream::WARNING);
    }
    std::ostream& EV_LOGE(Event &e) {
        return EV_LOG(e, LogStream::ERROR);
    }
    std::unique_ptr<Message> mMsg;
    std::map<Event::Type, std::list<EventCallback_t>> mEventCallbacks;

private:
    std::string mBuffer;
    std::string mKey;
};

} // namespace tuya
