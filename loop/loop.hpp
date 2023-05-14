#pragma once

#include <list>
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
        if (mHandlers.count(fd)) {
            LOGE() << "fd " << fd << " already registered" << std::endl;
            return -EALREADY;
        }

        mHandlers[fd] = handler;

        return 0;
    }

    int detach(int fd) {
        if (!mHandlers.count(fd))
            return -ENOENT;

        mHandlers.erase(fd);

        return 0;
    }

    Handler* getHandler(int fd) {
        return mHandlers.at(fd);
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
            LOGE() << "select() failed: " << ret << std::endl;
            return ret;
        } else {
            std::list<int> removeList;

            /* read data from all readable FDs */
            std::set<int> processedFds = { -1 }; // ignore promiscuous handlers
            for (const auto &it : mHandlers) {
                const auto &fd = it.first;
                if (processedFds.count(fd))
                    continue;

                if (!FD_ISSET(fd, &readFds))
                    continue;

                struct sockaddr_in addr;
                unsigned slen=sizeof(sockaddr);
                int ret = recvfrom(fd, const_cast<char*>(mBuffer.data()), mBuffer.length(), 0, (struct sockaddr *)&addr, &slen);
                if (ret <= 0) {
                    CloseEvent ce(fd, verbose);
                    it.second->handle(ce);
                    removeList.push_back(fd);
                    processedFds.insert(fd);
                    continue;
                }

                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);

                /* call all handlers listening on this FD */
                const std::string& data = mBuffer.substr(0, ret);
                ReadEvent e(fd, data, addr_str, verbose);
                for (auto &hIt : mHandlers)
                    if (hIt.first == fd)
                        hIt.second->handle(e);
                for (auto &hIt : mExtraHandlers)
                    hIt->handle(e);

                processedFds.insert(fd);
            }

            for (const auto &fd : removeList) {
                close(fd);
                mHandlers.erase(fd);
            }
        }

        return 0;
    }

private:
    LOG_MEMBERS(LOOP);

    std::string mBuffer;
    std::map<int, Handler*> mHandlers;
    std::set<Handler*> mExtraHandlers;
};

} // namespace tuya
