#pragma once

#include <map>
#include <set>

#include <errno.h>
#include <sys/select.h>
#include <unistd.h>

#include "event.hpp"
#include "handler.hpp"
#include "../logging.hpp"

namespace tuya {

class Loop {
    static const size_t BUFFER_SIZE = 1024;

public:
    Loop() : mBuffer("\0", BUFFER_SIZE) {
    }

    void attachExtra(Handler* handler) {
        mExtraHandlers.insert(handler);
    }

    void detachExtra(Handler* handler) {
        mExtraHandlers.erase(handler);
    }

    int attach(int fd, Handler* handler) {
        return attach(fd, "0.0.0.0", handler);
    }

    int attach(int fd, std::string addr, Handler* handler) {
        if (mHandlers.count(fd)) {
            LOGE() << "fd " << fd << " already registered" << std::endl;
            return -EALREADY;
        }

        mHandlers[fd] = std::make_pair<std::string, Handler*>(std::move(addr), std::move(handler));

        return 0;
    }

    int detach(int fd) {
        if (!mHandlers.count(fd))
            return -ENOENT;

        mHandlers.erase(fd);

        return 0;
    }

    Handler* getHandler(int fd) {
        return mHandlers.at(fd).second;
    }

    int handleEvent(Event&& e) {
        EV_LOGD(e) << "handling event: " << static_cast<std::string>(e) << std::endl;

        int ret = 0;
        auto handler = mHandlers.find(e.fd);
        if (handler != mHandlers.end())
            ret -= handler->second.second->handle(e);

        for (auto &hIt : mExtraHandlers)
            ret -= hIt->handle(e);

        return ret;
    }

    int loop(unsigned int timeoutMs = 1000, LogStream::Level logLevel = LogStream::INFO) {
        fd_set readFds;
        FD_ZERO(&readFds);
        int maxFd = 0;

        for (const auto &it : mHandlers) {
            /* run heartBeat function of all handlers, regardless of socket state */
            it.second.second->heartBeat();

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
            LOGE() << "select() failed: " << ret << std::endl;
            return ret;
        } else {
            /* read data from all readable FDs */
            std::set<int> processFds;
            for (const auto &it : mHandlers)
                if (it.first != -1) // ignore promiscuous handlers
                    processFds.insert(it.first);

            for (const auto &fd : processFds) {
                if (!FD_ISSET(fd, &readFds))
                    continue;

                struct sockaddr_in addr;
                unsigned slen = sizeof(sockaddr);

                // TODO: this is SocketHandler specific
                int ret = recvfrom(fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
                if ((ret > 0) && slen) {
                    char addr_str[INET_ADDRSTRLEN] = { 0 };
                    inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);
                    if (mHandlers.at(fd).first != addr_str)
                        mHandlers.at(fd).first.assign(addr_str);
                }

                if (ret <= 0) {
                    ret = handleEvent(CloseEvent(fd, mHandlers.at(fd).first, logLevel));
                } else {
                    /* call all handlers listening on this FD */
                    const std::string& data = mBuffer.substr(0, ret);
                    ret = handleEvent(ReadEvent(fd, data, mHandlers.at(fd).first, logLevel));
                }
                if (ret == -1) {
                    mHandlers.erase(fd);
                }
            }
        }

        return 0;
    }

private:
    LOG_MEMBERS(LOOP);

    std::string mBuffer;
    std::map<int, std::pair<std::string, Handler*>> mHandlers;
    std::set<Handler*> mExtraHandlers;
};

} // namespace tuya
