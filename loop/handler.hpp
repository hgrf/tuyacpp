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

    static const size_t BUFFER_SIZE = 1024;

public:
    Handler(const std::string& key = Message::DEFAULT_KEY) : mBuffer("\0", BUFFER_SIZE), mKey(key) {
    }

    int handle(Event e) {
        int ret = 0;
        EV_LOGD(e) << "handling " << std::string(e) << std::endl;;

        switch (e.type)
        {
        case Event::READ:
            ret = handleRead(e);
            break;
        case Event::CLOSING:
            ret = handleClose(e);
            break;
        default:
            break;
        }

        if (ret < 0)
            EV_LOGW(e) << "cb() failed: " << ret << std::endl;

        return ret;
    }

    int handleRead(Event e) {
        struct sockaddr_in addr;
        unsigned slen=sizeof(sockaddr);
        int ret = recvfrom(e.fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
        if (ret <= 0)
            return -EINVAL;

        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

        const std::string& raw = mBuffer.substr(0, ret);
        if (raw.length() < 2 * sizeof(uint32_t))
            throw std::runtime_error("message too short");

        uint32_t prefix = ntohl(*reinterpret_cast<const uint32_t*>(raw.data()));
        switch(prefix) {
        case Message55AA::PREFIX:
            mMsg = std::make_unique<Message55AA>(raw, mKey, false);
            break;
        default:
            throw std::runtime_error("unknown prefix");
        }

        return handleRead(e, addr_str, mMsg->data());
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
    LOG_MEMBERS("HANDLER");
    std::unique_ptr<Message> mMsg;

private:
    std::string mBuffer;
    std::string mKey;
};

} // namespace tuya
