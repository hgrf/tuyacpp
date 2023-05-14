#pragma once

#include <iostream>
#include <memory>

#include <netinet/in.h>

#include <CRC.h>
#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

#include "message.hpp"
#include "../logging.hpp"

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

    Message55AA(uint32_t seqNo, uint32_t cmd, const ordered_json& data) :
        Message(PREFIX, seqNo, cmd, data) {
    }

    Message55AA(const std::string& raw, const std::string& key = DEFAULT_KEY, bool noRetCode = false) :
        Message(0, 0, 0, ordered_json{{}}) {
        const size_t headerLen = noRetCode ? (sizeof(Header) - sizeof(uint32_t)) : sizeof(Header);
        const size_t footerLen = sizeof(Footer);
        if (raw.length() < headerLen + footerLen)
            throw std::runtime_error("message too short");

        const Header *header = reinterpret_cast<const Header *>(raw.data());
        mPrefix = ntohl(header->prefix);
        mSeqNo = ntohl(header->seqNo);
        mCmd = ntohl(header->cmd);
        uint32_t payloadLen = ntohl(header->payloadLen);

        if (raw.length() != sizeof(Header) - sizeof(uint32_t) + payloadLen)
            throw std::runtime_error("invalid message length");

        const Footer *footer = reinterpret_cast<const Footer *>(raw.data() + raw.length() - sizeof(Footer));
        if (ntohl(footer->suffix) != SUFFIX)
            throw std::runtime_error("invalid suffix");

        uint32_t crc = CRC::Calculate(raw.data(), raw.length() - sizeof(Footer), CRC::CRC_32());
        if (ntohl(footer->crc) != crc)
            throw std::runtime_error("invalid CRC");

        auto result = decrypt(std::string(raw.data() + headerLen, payloadLen), key);
        if (result.length())
        {
            try {
                mData = ordered_json::parse(result);
            } catch (const ordered_json::parse_error& e) {
                LOGE() << "Failed to parse payload. Message: " << (const std::string&) *this << std::endl;
                mData = ordered_json();
            }
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

} // namespace tuya
