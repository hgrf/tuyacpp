#pragma once

#include <functional>
#include <iostream>
#include <list>
#include <map>

#include <stddef.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>

#include "protocol/message.hpp"

namespace tuya {

class Loop {
public:
    class Event;
    class Handler;
    typedef std::function<int(int, Loop::Event, bool)> EventCallback_t;

    class Event {
    public:
        enum Type : uint8_t {
            READ,
            CLOSING,
        };

        Event(int fd, Type t) : mFd(fd), mType(t) {}

        const int& fd() const {
            return mFd;
        }

        const Type& type() const {
            return mType;
        }

        const std::string& typeStr() const {
            static const std::string invalidStr = "INVALID";
            static const std::map<Type, std::string> map = {
                {READ, "READ"},
                {CLOSING, "CLOSING"}
            };
            const auto& it = map.find(mType);
            if (it == map.end())
                return invalidStr;
            return it->second;
        }

        operator std::string() const {
            return "Event { fd: " + std::to_string(mFd) + ", type: " + typeStr() + "}";
        }

    private:
        int mFd;
        Type mType;
    };


    class Handler {
        static const size_t BUFFER_SIZE = 1024;

    public:
        Handler(const std::string& key = DEFAULT_KEY) : mKey(key) {
            mBuffer.resize(BUFFER_SIZE);
        }

        int handle(Event e, bool verbose = true) {
            if (verbose)
                std::cout << "Handling " << std::string(e) << std::endl;

            switch(e.type()) {
            case Event::READ:
                return _handleRead(e, verbose);
            case Event::CLOSING:
                return _handleClose(e, verbose);
            default:
                return -EINVAL;
            }
        }

        virtual int handleRead(Event e, bool verbose) {
            (void) e, (void) verbose;
            return 0;
        }

        virtual int handleClose(Event e, bool verbose) {
            (void) e, (void) verbose;
            return 0;
        }

        virtual int heartBeat() {
            return 0;
        }

    private:
        int _handleClose(Event e, bool verbose) {
            if (verbose)
                std::cout << "socket is closing" << std::endl;

            return handleClose(e, verbose);
        }

        int _handleRead(Event e, bool verbose) {
            struct sockaddr_in addr;
            unsigned slen=sizeof(sockaddr);
            int ret = recvfrom(e.fd(), const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
            if (ret <= 0)
                return -EINVAL;

            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

            mMsg = Message::deserialize(mBuffer.substr(0, ret), mKey);
            if (verbose)
                std::cout << "received " << ret  << " bytes from " << addr_str << ": " << std::string(*mMsg) << std::endl;

            return handleRead(e, verbose);
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

        return 0;
    }

    int loop(unsigned int timeoutMs = 1000) {
        fd_set readFds;
        FD_ZERO(&readFds);
        int maxFd = 0;

        for (const auto &it : mHandlers) {
            /* run heartBeat function of all handlers, regardless of socket state */
            it.second->heartBeat();

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
            std::list<int> removeList;
            for (auto &it : mHandlers) {
                const auto &fd = it.first;
                if (FD_ISSET(fd, &readFds)) {
                    int handleRet = it.second->handle(Event(fd, Event::READ));
                    if (handleRet < 0) {
                        it.second->handle(Event(fd, Event::CLOSING));
                        removeList.push_back(fd);
                    }
                    ret--;
                }

                if (!ret)
                    break;
            }
            for (const auto &fd : removeList) {
                close(fd);
                mHandlers.erase(fd);
            }
        }

        return 0;
    }

private:
    std::map<int, Handler*> mHandlers;
};

} // namespace tuya
