#pragma once

#include <map>
#include <iostream>

namespace tuya {

class LogStream : public std::ostream {
public:
    enum Level { DEBUG, INFO, WARNING, ERROR };
    static const std::map<Level, std::string> ColorMap;
    static const std::string RESET_COLOR;

    // TODO: implement tag-specific log levels
    LogStream(const std::string& tag) : mTag(tag) {}

    static LogStream& make(const std::string& t) {
        auto result = mLogStreams.emplace(t, t);
        return result.second ? result.first->second : mNullStream;
    }

    static LogStream& get(const std::string& t, Level l) {
        if (!t.size())
            return mNullStream;

        auto it = mLogStreams.find(t);
        LogStream& s = (it != mLogStreams.end()) ? it->second : make(t);
        s << ColorMap.at(l) << "[" << t << "] " << RESET_COLOR;
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
    static LogStream mNullStream;
    static std::map<std::string, LogStream> mLogStreams;
};

LogStream LogStream::mNullStream("");
const std::map<LogStream::Level, std::string> LogStream::ColorMap{{
    {LogStream::DEBUG, "\e[1;35m"},
    {LogStream::INFO, "\e[1;32m"},
    {LogStream::WARNING, "\e[1;33m"},
    {LogStream::ERROR, "\e[1;31m"},
}};
const std::string LogStream::RESET_COLOR("\e[0m");
std::map<std::string, LogStream> LogStream::mLogStreams = {};

#define LOG_MEMBERS(t) \
    virtual const std::string& TAG() { static const std::string tag = #t; return tag; }; \
    LogStream& LOGD() { return LogStream::get(TAG(), LogStream::DEBUG); } \
    LogStream& LOGI() { return LogStream::get(TAG(), LogStream::INFO); } \
    LogStream& LOGW() { return LogStream::get(TAG(), LogStream::WARNING); } \
    LogStream& LOGE() { return LogStream::get(TAG(), LogStream::ERROR); } \
    std::ostream& EV_LOGD(Event &e) { return e.log(LogStream::get(TAG(), LogStream::DEBUG)); } \
    std::ostream& EV_LOGI(Event &e) { return e.log(LogStream::get(TAG(), LogStream::INFO)); } \
    std::ostream& EV_LOGW(Event &e) { return e.log(LogStream::get(TAG(), LogStream::WARNING)); } \
    std::ostream& EV_LOGE(Event &e) { return e.log(LogStream::get(TAG(), LogStream::ERROR)); }

}   // namespace tuya
