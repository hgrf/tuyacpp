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

    Message55AA(const std::string& raw, uint32_t& parsedSize, const std::string& key = DEFAULT_KEY, bool noRetCode = false) :
        Message(0, 0, 0, ordered_json{{}}) {
        static const std::map<int, const std::string> dpsToString = {
            {1, "is_on"},
            {2, "brightness"}, // {2, "mode"},
            {3, "colourtemp"}, // {3, "brightness"},
            // {4, "colourtemp"},
            // {5, "colour"},
            {20, "is_on"},
            {21, "mode"},
            {22, "brightness"},
            {23, "colourtemp"},
            {24, "colour"},
        };
        const size_t headerLen = noRetCode ? (sizeof(Header) - sizeof(uint32_t)) : sizeof(Header);
        if (raw.length() < headerLen + sizeof(Footer))
            throw std::runtime_error("message too short");

        const Header *header = reinterpret_cast<const Header *>(raw.data());
        mPrefix = ntohl(header->prefix);
        mSeqNo = ntohl(header->seqNo);
        mCmd = ntohl(header->cmd);
        if (!noRetCode)
            mRetCode = ntohl(header->retCode);
        uint32_t payloadLen = ntohl(header->payloadLen);
        uint32_t dataLen = sizeof(Header) + payloadLen - sizeof(uint32_t);

        if (raw.length() < dataLen)
            throw std::runtime_error("not enough data");

        const Footer *footer = reinterpret_cast<const Footer *>(raw.data() + dataLen - sizeof(Footer));
        if (ntohl(footer->suffix) != SUFFIX)
            throw std::runtime_error("invalid suffix");

        uint32_t crc = CRC::Calculate(raw.data(), dataLen - sizeof(Footer), CRC::CRC_32());
        if (ntohl(footer->crc) != crc)
            throw std::runtime_error("invalid CRC");

        parsedSize = dataLen;
        const auto& payload = std::string(raw.data() + headerLen, payloadLen - sizeof(uint32_t) - sizeof(Footer));
        if (payload.length()) {
            auto result = decrypt(payload.substr(payloadPrefix().length()), key);
            if (result.length()) {
                try {
                    mData = ordered_json::parse(result);
                    if (mData.contains("dps")) {
                        auto dps = mData["dps"];
                        for (auto it = dps.begin(); it != dps.end(); ++it) {
                            auto dpsString = dpsToString.find(std::stoi(it.key()));
                            if (dpsString != dpsToString.end())
                                mData[dpsString->second] = it.value();
                        }
                    }
                } catch (const ordered_json::parse_error& e) {
                    LOGE() << "Failed to parse " << (const std::string&) *this << " payload: " << result << std::endl;
                    mData = ordered_json();
                }
            } else {
                LOGE() << "Failed to decrypt " << (const std::string&) *this << " payload: " << payload << std::endl;
            }
        } else {
            mData = ordered_json::object();
        }
    }

   virtual std::string serialize(const std::string& key = DEFAULT_KEY, bool noRetCode = true) override {
        // TODO: demystify the three different payloadLen...

        std::string result;

        std::string payload = payloadPrefix() + encrypt(mData.dump(), key);
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
    std::string payloadPrefix() {
        static const char PREFIX_VER_3_3[] = "3.3\0\0\0\0\0\0\0\0\0\0\0\0";

        switch(mCmd) {
        case DP_QUERY:
        case UDP_NEW:
            return "";
        default:
            return std::string(PREFIX_VER_3_3, sizeof(PREFIX_VER_3_3) - 1);
        }
    }
};

} // namespace tuya
