#pragma once

#include <iostream>
#include <map>

#include <stddef.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "protocol/message.hpp"

namespace tuya {

class Loop {
public:
    class Event {
    public:
        enum Type : uint8_t {
            READ,
        };

        Event(Type t) : mType(t) {}

        operator Type() const {
            return mType;
        }

        operator std::string() const {
            switch(mType) {
            case READ:
                return "READ";
            default:
                return "INVALID";
            }
        }

    private:
        Type mType;
    };


    class Handler {
        static const size_t BUFFER_SIZE = 1024;

    public:
        Handler(const std::string& key = DEFAULT_KEY) : mKey(key) {
            mBuffer.resize(BUFFER_SIZE);
        }

        virtual int handle(int fd, Event e, bool verbose = true) {
            if (verbose)
                std::cout << "Handling event from fd " << fd << ": " << std::string(e) << std::endl;
            switch(Event::Type(e))
            {
            case Event::READ: {
                struct sockaddr_in addr;
                unsigned slen=sizeof(sockaddr);
                int ret = recvfrom(fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
                if (ret <= 0) {
                    std::cerr << "recfrom() -> " << ret << std::endl;
                    break;
                }

                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

                mMsg = Message::deserialize(mBuffer.substr(0, ret), mKey);
                if (verbose)
                    std::cout << "received " << ret  << " bytes from " << addr_str << ": " << std::string(*mMsg) << std::endl;
                break;
            }
            default:
                throw std::runtime_error("Unimplemented event");
            }
        }

    protected:
        std::unique_ptr<Message> mMsg;

    private:
        std::string mBuffer;
        std::string mKey;
    };

    int attach(int fd, Handler* handler) {
        if (mHandlers.count(fd))
            return -EALREADY;

        mHandlers[fd] = handler;

        return 0;
    }

    int detach(int fd) {
        if (!mHandlers.count(fd))
            return -ENOENT;

        mHandlers.erase(fd);
    }

    int loop(unsigned int timeoutMs = 1000) {
        if (!mHandlers.size()) {
            std::cerr << "no handler defined" << std::endl;
            return -EINVAL;
        }

        fd_set readFds;
        FD_ZERO(&readFds);
        int maxFd = 0;
        for (const auto &it : mHandlers) {
            FD_SET(it.first, &readFds);
            if (it.first > maxFd)
                maxFd = it.first;
        }

        struct timeval tv = {
            .tv_sec = timeoutMs / 1000,
            .tv_usec = (timeoutMs % 1000 ) * 1000,
        };

        int ret = select(++maxFd, &readFds, NULL, NULL, &tv);
        if (ret < 0) {
            std::cerr << "select() failed: " << ret << std::endl;
            return ret;
        } else {
            for (auto &it : mHandlers) {
                if (FD_ISSET(it.first, &readFds)) {
                    it.second->handle(it.first, Event::READ);
                    ret--;
                }

                if (!ret)
                    break;
            }
        }
    }

private:
    std::map<int, Handler*> mHandlers;
};

} // namespace tuya
