#pragma once

#include <map>
#include <string>

#include "../logging.hpp"
#include "../protocol/message.hpp"

namespace tuya {

class Event {
public:
    enum Type : uint8_t {
        INVALID,
        CONNECTED,
        READABLE,
        READ,
        MESSAGE,
        CLOSING,
    };
    const int fd;
    const Type type;
    const LogStream::Level logLevel;

    const std::string& typeStr() const {
        static const std::map<Type, std::string> map = {
            {INVALID, "INVALID"},
            {CONNECTED, "CONNECTED"},
            {READABLE, "READABLE"},
            {READ, "READ"},
            {MESSAGE, "MESSAGE"},
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

    std::ostream &log(const std::string& tag, LogStream::Level level) {
        if (level >= logLevel)
            return LogStream::get(tag, level) << "[EV " << typeStr() << "(" << fd << ")] ";
        else
            return LogStream::nullstream();
    }

    virtual ~Event() = default;

protected:
    Event(int f, Type t, LogStream::Level l) : fd(f), type(t), logLevel(l) {}

private:
    static std::map<std::string, LogStream> mLogStreams;
};

class ConnectedEvent : public Event {
public:
    ConnectedEvent(int f, LogStream::Level l) : Event(f, Event::CONNECTED, l) {}
};

class ReadableEvent : public Event {
public:
    ReadableEvent(int f, LogStream::Level l) : Event(f, Event::READABLE, l) {}
};

class ReadEvent : public Event {
public:
    const std::string &addr;
    const std::string &data;

    ReadEvent(int f, const std::string &d, const std::string &a, LogStream::Level l) : Event(f, Event::READ, l), addr(a), data(d) {}
};

class MessageEvent : public Event {
public:
    const std::string &addr;
    const Message& msg;

    MessageEvent(int f, const Message& m, const std::string& a, LogStream::Level l) : Event(f, Event::MESSAGE, l), addr(a), msg(m) {}
};

class CloseEvent : public Event {
public:
    const std::string &addr;

    CloseEvent(int f, const std::string &a, LogStream::Level l) : Event(f, Event::CLOSING, l), addr(a) {}
};

}  // namespace tuya
