#pragma once

#include "message.hpp"
#include "message55aa.hpp"

namespace tuya {

std::unique_ptr<Message> Message::deserialize(const std::string& raw, const std::string& key, bool noRetCode)
{
    if (raw.length() < 2 * sizeof(uint32_t))
        throw std::runtime_error("message too short");

    uint32_t prefix = ntohl(*reinterpret_cast<const uint32_t*>(raw.data()));
    switch(prefix) {
    case Message55AA::PREFIX:
        return std::make_unique<Message55AA>(raw, key, noRetCode);
    default:
        std::cerr << "unknown prefix: 0x" << std::hex << prefix << std::endl;
        return nullptr;
    }
}

}; // namespace tuya