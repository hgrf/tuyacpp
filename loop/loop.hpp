#pragma once

#include <list>
#include <map>
#include <queue>
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

    struct OrderByDeadline {
        bool operator() (const DelayedWork& work1, const DelayedWork& work2) {
            return work2.deadline < work1.deadline;
        }
    };

public:
    Loop() {
    }

    void attach(Handler* handler) {
        mExtraHandlers.insert(handler);
    }

    void detach(Handler* handler) {
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

    int attachWritable(int fd, Handler* handler) {
        if (mWritableHandlers.count(fd)) {
            LOGE() << "fd " << fd << " already registered" << std::endl;
            return -EALREADY;
        }

        mWritableHandlers[fd] = handler;

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
        mWork.push(DelayedWork(std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs), work));
    }

    void handleEvent(Event&& e) {
        auto handler = mHandlers.find(e.fd);
        if (handler != mHandlers.end())
            handler->second->handle(e);

        for (auto &hIt : mExtraHandlers)
            hIt->handle(e);
    }

    int loop(unsigned int timeoutMs = 1000, LogStream::Level logLevel = LogStream::INFO) {
        int delayMs = timeoutMs;
        while (mWork.size()) {
            const auto& nextWork = mWork.top();
            const auto& delay = nextWork.deadline - std::chrono::steady_clock::now();
            delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();
            if (delayMs <= 0) {
                LOGD() << "executing scheduled work" << std::endl;
                nextWork.work();
                mWork.pop();
            } else {
                LOGD() << "work scheduled in " << delayMs << " ms" << std::endl;
                break;
            }
        }
        timeoutMs = (delayMs < (int) timeoutMs) ? delayMs : timeoutMs;

        fd_set readFds;
        fd_set writeFds;
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        int maxFd = 0;
        int maxWritableFd = 0;

        for (const auto &it : mHandlers) {
            FD_SET(it.first, &readFds);
            if (it.first > maxFd)
                maxFd = it.first;
        }

        for (const auto &it : mWritableHandlers) {
            FD_SET(it.first, &writeFds);
            if (it.first > maxFd)
                maxWritableFd = it.first;
        }
        maxFd = (maxWritableFd > maxFd) ? maxWritableFd : maxFd;

        struct timeval tv = {
            .tv_sec = timeoutMs / 1000,
            .tv_usec = (timeoutMs % 1000 ) * 1000,
        };

        int ret = select(++maxFd, &readFds, &writeFds, NULL, &tv);
        LOGD() << "select done, " << ret << " fds readable" << std::endl;
        if (ret < 0) {
            LOGE() << "select() failed: " << ret << std::endl;
            return ret;
        } else {
            /* read data from all readable FDs */
            for (const auto &it : mHandlers) {
                const auto &fd = it.first;
                if (FD_ISSET(fd, &readFds))
                    handleEvent(ReadableEvent(fd, logLevel));
            }
            std::list<int> removeList;
            for (const auto &it : mWritableHandlers) {
                const auto &fd = it.first;
                if (FD_ISSET(fd, &writeFds)) {
                    WritableEvent e(fd, logLevel);
                    mWritableHandlers[fd]->handle(e);
                    removeList.push_back(fd);
                }
            }
            for (int fd : removeList)
                mWritableHandlers.erase(fd);
        }

        return 0;
    }

private:
    LOG_MEMBERS(LOOP);

    std::priority_queue<DelayedWork, std::vector<DelayedWork>, OrderByDeadline> mWork;
    std::map<int, Handler*> mHandlers;
    std::map<int, Handler*> mWritableHandlers;
    std::set<Handler*> mExtraHandlers;
};

} // namespace tuya
