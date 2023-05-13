#pragma once

#include <map>
#include <string>

#include "../logging.hpp"

namespace tuya {

class Event {
public:
    enum Type : uint8_t {
        INVALID,
        READ,
        CLOSING,
    };
    const int fd;
    const Type type;

    Event(int f, Type t, bool v) : fd(f), type(t), mVerbose(v) {}

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
        return "Event {fd=" + std::to_string(fd) + ", type=" + typeStr() + "}";
    }

    std::ostream &log(LogStream& logger) {
        return !mVerbose ? LogStream::get("", LogStream::ERROR) : logger << "[EV " << typeStr() << "(" << fd << ")] ";
    }

private:
    static std::map<std::string, LogStream> mLogStreams;
    const bool mVerbose;
};

}  // namespace tuya
