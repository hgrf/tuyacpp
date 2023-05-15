#pragma once

#include <chrono>
#include <ctime>
#include <map>
#include <iostream>

namespace tuya {

class LogStream : public std::ostream {
public:
    enum Level { DEBUG, INFO, WARNING, ERROR };

    // TODO: implement tag-specific log levels
    LogStream(const std::string& tag) : mTag(tag) {}

    static std::map<std::string, LogStream>& logstreams() {
        static std::map<std::string, LogStream> sLogStreams = {};
        return sLogStreams;
    }

    static LogStream& nullstream() {
        static LogStream sNullStream("");
        return sNullStream;
    }

    static LogStream& make(const std::string& t) {
        auto result = logstreams().emplace(t, t);
        return result.second ? result.first->second : nullstream();
    }

    static LogStream& get(const std::string& t, Level l) {
        static const std::map<LogStream::Level, std::string> levelMap = {
            {LogStream::DEBUG, "DEBUG"},
            {LogStream::INFO, "INFO"},
            {LogStream::WARNING, "WARNING"},
            {LogStream::ERROR, "ERROR"},
        };
        static const std::map<LogStream::Level, std::string> colorMap = {
            {LogStream::DEBUG, "\e[1;35m"},
            {LogStream::INFO, "\e[1;32m"},
            {LogStream::WARNING, "\e[1;33m"},
            {LogStream::ERROR, "\e[1;31m"},
        };
        static const std::string resetColor = "\e[0m";

        if (!t.size())
            return nullstream();

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        const auto& now_ctime = std::ctime(&now_time);
        const auto& levelStr = levelMap.at(l);
        auto it = logstreams().find(t);
        LogStream& s = (it != logstreams().end()) ? it->second : make(t);
        s << colorMap.at(l) << "[" << std::string(now_ctime + 11, now_ctime + 19) << " " << levelStr << " " << t << "] " << resetColor;
        return s;
    }

    template <typename T>
    std::ostream& operator<<(T const &t) {
        return !mTag.size() ? *this : std::cout << t;
    }

    std::ostream& operator<<(std::ostream& s) {
        return s;
    }

private:
    std::string mTag;
};

#define LOG_MEMBERS(t) \
    virtual const std::string& TAG() { static const std::string tag = #t; return tag; }; \
    LogStream& LOGD() { return LogStream::get(TAG(), LogStream::DEBUG); } \
    LogStream& LOGI() { return LogStream::get(TAG(), LogStream::INFO); } \
    LogStream& LOGW() { return LogStream::get(TAG(), LogStream::WARNING); } \
    LogStream& LOGE() { return LogStream::get(TAG(), LogStream::ERROR); } \
    std::ostream& EV_LOGD(Event &e) { return e.log(TAG(), LogStream::DEBUG); } \
    std::ostream& EV_LOGI(Event &e) { return e.log(TAG(), LogStream::INFO); } \
    std::ostream& EV_LOGW(Event &e) { return e.log(TAG(), LogStream::WARNING); } \
    std::ostream& EV_LOGE(Event &e) { return e.log(TAG(), LogStream::ERROR); }

}   // namespace tuya
