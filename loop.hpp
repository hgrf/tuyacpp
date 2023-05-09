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
    typedef std::function<int(Loop::Event)> EventCallback_t;

    class Event {
    public:
        enum Type : uint8_t {
            INVALID,
            READ,
            CLOSING,
        };
        const int fd;
        const Type type;
        const bool verbose;

        Event(int f, Type t, bool v) : fd(f), type(t), verbose(v) {}

        const std::string& typeStr() const {
            static const std::map<Type, std::string> map = {
                {INVALID, "INVALID"},
                {READ, "READ"},
                {CLOSING, "CLOSING"}
            };
            const auto& it = map.find(type);
            if (it == map.end())
                return map.at(INVALID);
            return it->second;
        }

        operator std::string() const {
            return "Event { fd: " + std::to_string(fd) + ", type: " + typeStr() + " }";
        }
    };


    class Handler {
        static const size_t BUFFER_SIZE = 1024;

    public:
        Handler(const std::string& key = DEFAULT_KEY) : mKey(key) {
            mBuffer.resize(BUFFER_SIZE);

            registerEventCallback(Event::READ, [this](Event e) {
                struct sockaddr_in addr;
                unsigned slen=sizeof(sockaddr);
                int ret = recvfrom(e.fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
                if (ret <= 0)
                    return -EINVAL;

                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

                mMsg = Message::deserialize(mBuffer.substr(0, ret), mKey);
                if (e.verbose)
                    std::cout << "[HANDLER] received " << ret  << " bytes from " << addr_str << ": " << mMsg->data() << std::endl;

                return handleRead(e);
            });

            registerEventCallback(Event::CLOSING, [this](Event e) {
                if (e.verbose)
                    std::cout << "socket is closing" << std::endl;

                return handleClose(e);
            });
        }

        void registerEventCallback(Loop::Event::Type t, EventCallback_t cb) {
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
            if (e.verbose)
                std::cout << "[HANDLER] Handling " << std::string(e) << std::endl;

            const auto& it = mEventCallbacks.find(e.type);
            if (it == mEventCallbacks.end())
                return 0;

            for (const auto &cb : it->second) {
                ret = cb(e);
                if (ret < 0) {
                    std::cerr << "cb() failed: " << ret << std::endl;
                    return ret;
                }
            }

            return 0;
        }

        virtual int handleRead(Event e) {
            (void) e;
            return 0;
        }

        virtual int handleClose(Event e) {
            (void) e;
            return 0;
        }

        virtual int heartBeat() {
            return 0;
        }

    protected:
        std::unique_ptr<Message> mMsg;
        std::map<Loop::Event::Type, std::list<EventCallback_t>> mEventCallbacks;

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

    int loop(unsigned int timeoutMs = 1000, bool verbose = true) {
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
                    int handleRet = it.second->handle(Event(fd, Event::READ, verbose));
                    if (handleRet < 0) {
                        it.second->handle(Event(fd, Event::CLOSING, verbose));
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
