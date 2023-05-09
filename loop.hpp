#pragma once

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
    class Event {
    public:
        enum Type : uint8_t {
            READ,
            CLOSING,
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

        virtual int handleRead(int fd, Event e, bool verbose) {
            return 0;
        }

        virtual int handleClose(int fd, Event e, bool verbose) {
            return 0;
        }

        virtual int handle(int fd, Event e, bool verbose = true) {
            int ret = 0;

            if (verbose)
                std::cout << "Handling event from fd " << fd << ": " << std::string(e) << std::endl;

            switch(Event::Type(e))
            {
            case Event::READ: {
                struct sockaddr_in addr;
                unsigned slen=sizeof(sockaddr);
                ret = recvfrom(fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
                if (ret <= 0) {
                    if (ret == 0)
                        ret = -EINVAL;
                    break;
                }

                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

                mMsg = Message::deserialize(mBuffer.substr(0, ret), mKey);
                if (verbose)
                    std::cout << "received " << ret  << " bytes from " << addr_str << ": " << std::string(*mMsg) << std::endl;

                ret = handleRead(fd, e, verbose);
                break;
            }
            case Event::CLOSING:
                if (verbose)
                    std::cout << "socket is closing" << std::endl;

                ret = handleClose(fd, e, verbose);
                break;
            default:
                std::cerr << "unimplemented event: " << (int) Event::Type(e) << std::endl;
                ret = -EINVAL;
            }

            return ret;
        }

        virtual int heartBeat() {

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
                if (FD_ISSET(it.first, &readFds)) {
                    int handleRet = it.second->handle(it.first, Event::READ);
                    if (handleRet < 0) {
                        it.second->handle(it.first, Event::CLOSING);
                        removeList.push_back(it.first);
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
    }

private:
    std::map<int, Handler*> mHandlers;
};

} // namespace tuya
