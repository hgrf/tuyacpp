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
    struct DelayedWork {
        DelayedWork(std::chrono::time_point<std::chrono::steady_clock> d, std::function<void()>& w) : deadline(d), work(w) {}
        std::chrono::time_point<std::chrono::steady_clock> deadline;
        std::function<void()> work;
    };

public:
    Loop() {
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

    void pushWork(std::function<void()>&& work, uint32_t delayMs = 0) {
        mWork.push_back(DelayedWork(std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs), work));
    }

    void handleEvent(Event&& e) {
        auto handler = mHandlers.find(e.fd);
        if (handler != mHandlers.end())
            handler->second->handle(e);

        for (auto &hIt : mExtraHandlers)
            hIt->handle(e);
    }

    int loop(unsigned int timeoutMs = 1000, LogStream::Level logLevel = LogStream::INFO) {
        for (;;) {
            auto it = mWork.begin();
            for (; it != mWork.end(); ++it) {
                if (it->deadline < std::chrono::steady_clock::now()) {
                    it->work();
                    break;
                }
            }
            if (it != mWork.end())
                mWork.erase(it);
            else
                break;
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
            LOGE() << "select() failed: " << ret << std::endl;
            return ret;
        } else {
            /* read data from all readable FDs */
            for (const auto &it : mHandlers) {
                const auto &fd = it.first;
                if (!FD_ISSET(fd, &readFds))
                    continue;

                handleEvent(ReadableEvent(fd, logLevel));
            }
        }

        return 0;
    }

private:
    LOG_MEMBERS(LOOP);

    std::list<DelayedWork> mWork;
    std::map<int, Handler*> mHandlers;
    std::set<Handler*> mExtraHandlers;
};

} // namespace tuya
