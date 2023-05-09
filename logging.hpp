#pragma once

#include <map>
#include <ostream>

namespace tuya {
class LogStream : public std::ostream {
public:
    // TODO: implement tag-specific log levels
    LogStream(const std::string& tag) : mTag(tag) {}

    static LogStream& make(const std::string& t) {
        auto result = mLogStreams.emplace(t, t);
        return result.second ? result.first->second : mNullStream;
    }

    static LogStream& get(const std::string& t) {
        if (!t.size())
            return mNullStream;

        auto it = mLogStreams.find(t);
        LogStream& s = (it != mLogStreams.end()) ? it->second : make(t);
        s << "[" << t << "] ";
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
std::map<std::string, LogStream> LogStream::mLogStreams = {};

}   // namespace tuya
