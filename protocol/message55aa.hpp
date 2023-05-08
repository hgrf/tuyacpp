#pragma once

#include <iostream>
#include <memory>

#include <netinet/in.h>

#include "CRCpp/inc/CRC.h"
#include "json/single_include/nlohmann/json.hpp"
using ordered_json = nlohmann::ordered_json;

#include "common.hpp"
#include "message.hpp"

namespace tuya {

class Message55AA : public Message {
public:
    static const uint32_t PREFIX = 0x55aa;
    static const uint32_t SUFFIX = 0xaa55;   

    struct Header {
        uint32_t prefix;
        uint32_t seqNo;
        uint32_t cmd;
        uint32_t payloadLen;
        union {
            uint32_t retCode;
        };
    };

    struct Footer {
        uint32_t crc;
        uint32_t suffix;
    };

    Message55AA(uint32_t prefix, uint32_t seqNo, uint32_t cmd, const ordered_json& data) :
        Message(prefix, seqNo, cmd, data) {
    }

    Message55AA(unsigned char *raw, size_t len, const std::string& key = DEFAULT_KEY, bool noRetCode = false) :
        Message(0, 0, 0, ordered_json{{}}) {
        const size_t headerLen = noRetCode ? (sizeof(Header) - sizeof(uint32_t)) : sizeof(Header);
        const size_t footerLen = sizeof(Footer);
        if (len < headerLen + footerLen)
            throw std::runtime_error("message too short");

        Header *header = reinterpret_cast<Header *>(raw);
        mPrefix = ntohl(header->prefix);
        mSeqNo = ntohl(header->seqNo);
        mCmd = ntohl(header->cmd);
        uint32_t payloadLen = ntohl(header->payloadLen);

        if (len != sizeof(Header) - sizeof(uint32_t) + payloadLen)
            throw std::runtime_error("invalid message length");

        Footer *footer = reinterpret_cast<Footer *>(raw + len - sizeof(Footer));
        if (ntohl(footer->suffix) != SUFFIX)
            throw std::runtime_error("invalid suffix");

        uint32_t crc = CRC::Calculate(raw, len - sizeof(Footer), CRC::CRC_32());
        if (ntohl(footer->crc) != crc)
            throw std::runtime_error("invalid CRC");

        auto result = decrypt(std::string((char *)raw + headerLen, payloadLen), key);
        std::cout << "Result: " << result << std::endl;

        try {
            mData = ordered_json::parse(result);
            std::cout << "Data: " << mData << std::endl;
        } catch (const ordered_json::parse_error& e) {
            std::cerr << "Failed to parse payload: " << result << std::endl;
            std::cerr << "  Message: " << (const std::string&) *this << std::endl;
            std::cerr << "  Error: " << e.what() << std::endl;
        }
    }

    std::string serialize(const std::string& key = DEFAULT_KEY, bool noRetCode = true) {
        // TODO: demystify the three different payloadLen...

        std::string result;

        std::string payload = encrypt(mData.dump(), key);
        const uint32_t payloadLen = payload.length() + (noRetCode ? 0 : 4);

        auto header = std::make_unique<Header>();
        header->prefix = htonl(PREFIX);
        header->seqNo = htonl(mSeqNo);
        header->cmd = htonl(mCmd);
        header->payloadLen = htonl(payloadLen + sizeof(Footer));
        header->retCode = 0;
        result += std::string((char *) header.get(), sizeof(Header) - (noRetCode ? 4 : 0));

        result += payload;

        uint32_t crc = CRC::Calculate(result.data(), sizeof(Header) - sizeof(uint32_t) + payloadLen, CRC::CRC_32());
        auto footer = std::make_unique<Footer>();
        footer->crc = htonl(crc);
        footer->suffix = htonl(SUFFIX);
        result += std::string((char *) footer.get(), sizeof(Footer));

        return result;
    }

private:
};

std::unique_ptr<Message> parse(unsigned char *raw, size_t len, const std::string& key = DEFAULT_KEY, bool noRetCode = false) {
    if (len < 2 * sizeof(uint32_t))
        throw std::runtime_error("message too short");

    uint32_t prefix = ntohl(*reinterpret_cast<uint32_t*>(raw));
    switch(prefix) {
    case Message55AA::PREFIX:
        return std::make_unique<Message55AA>(raw, len, key, noRetCode);
    default:
        std::cerr << "unknown prefix: 0x" << std::hex << prefix << std::endl;
        return nullptr;
    }
}

} // namespace tuya
