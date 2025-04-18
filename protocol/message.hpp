#pragma once

#include <iostream>
#include <sstream>

#ifdef TUYACPP_USE_MBEDTLS
    #include <mbedtls/aes.h>
#else
    #include <openssl/evp.h>
#endif

#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

#include "../logging.hpp"

namespace tuya {

class Message {
public:
    enum Command {
        CONTROL         = 0x07,
        STATUS          = 0x08,
        DP_QUERY        = 0x0a,
        UDP_NEW         = 0x13,
    };

    // md5(b"yGAdlopoPVldABfn").digest()
    static constexpr const char* DEFAULT_KEY = "l\x1e\xc8\xe2\xbb\x9b\xb5\x9a\xb5\x0b\r\xaf""d\x9b""A\n";

    Message(uint32_t prefix, uint32_t seqNo, uint32_t cmd, const ordered_json& data) :
    mPrefix(prefix),
    mSeqNo(seqNo),
    mCmd(cmd),
    mData(data) {

    }

    operator std::string() const {
        std::ostringstream ss;
        ss << std::hex << "Message { prefix: 0x" << mPrefix << ", seqno: 0x" << mSeqNo
           << ", cmd: 0x" << mCmd << std::dec << ", data: " << mData << " }";
        return ss.str();
    }


    bool hasData() {
        return !(mData.is_array() && (mData.size() == 1) && mData.at(0).is_null());
    }

    const ordered_json& data() const {
        return mData;
    }

    uint32_t prefix() const {
        return mPrefix;
    }

    uint32_t seqNo() const {
        return mSeqNo;
    }

    uint32_t cmd() const {
        return mCmd;
    }

    const std::string& cmdString() const {
        static const std::map<Command, const std::string> cmdToString = {
            { Command::CONTROL, "CONTROL" },
            { Command::DP_QUERY, "DP_QUERY" },
            { Command::STATUS, "STATUS" },
            { Command::UDP_NEW, "UDP_NEW" },
        };
        static const std::string unknownCommand = "UNKNOWN";
        auto it = cmdToString.find(static_cast<Command>(mCmd));
        if (it != cmdToString.end())
            return it->second;
        return unknownCommand;
    }

    virtual std::string serialize(const std::string& key = DEFAULT_KEY, bool noRetCode = true) = 0;

protected:
    LOG_MEMBERS(MESSAGE);

    std::string encrypt(const std::string& plain, const std::string& key) {
        std::string err;
        const char padNum = 16 - plain.length() % 16;
        std::string result = plain + std::string(padNum, padNum);

#ifdef TUYACPP_USE_MBEDTLS
        size_t p_len = result.length();
        int f_len = 0;
        mbedtls_aes_context aes_ctx;
        mbedtls_aes_init(&aes_ctx);
        if (mbedtls_aes_setkey_enc(&aes_ctx, (const unsigned char *) key.data(), 128) != 0) {
            err = "mbedtls_aes_setkey_enc failed";
        }
        if (!err.length()) {
            for (size_t i = 0; i < result.length(); i += 16) {
                if (mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, (const unsigned char *) result.data() + i, (unsigned char *) result.data() + i) != 0) {
                    err = "mbedtls_aes_crypt_ecb failed";
                    break;
                }
            }
        }
        mbedtls_aes_free(&aes_ctx);
#else
        int p_len = result.length();

        EVP_CIPHER_CTX* en = EVP_CIPHER_CTX_new();
        EVP_CIPHER_CTX_init(en);
        if(EVP_EncryptInit_ex(en, EVP_aes_128_ecb(), NULL, (const unsigned char *) key.data(), NULL) != 1) {
            err = "EVP_EncryptInit_ex failed";
        }
        if(!err.length() && EVP_EncryptUpdate(en, (unsigned char *) result.data(), &p_len, (const unsigned char *) result.data(), result.length()) != 1) {
            err = "EVP_EncryptUpdate failed";
        }
        // TODO: EVP_DecryptFinal_ex ?
        // if(EVP_DecryptFinal_ex(de, mPayload.get() + p_len, &f_len) != 1) {
        //     err = "EVP_DecryptFinal_ex failed" << std::endl;
        // }
        result.resize(p_len);
        EVP_CIPHER_CTX_free(en);
#endif

        if(err.length()) {
            result.clear();
            LOGE() << "encrypt() failed: " << err << std::endl;
        }

        return result;
    }

    std::string decrypt(const std::string& cipher, const std::string& key) {
        // TODO: padding operation should be symmetric with encrypt
        std::string err;
        std::string result = cipher;

#ifdef TUYACPP_USE_MBEDTLS
        size_t p_len = result.length();
        int f_len = 0;
        mbedtls_aes_context aes_ctx;
        mbedtls_aes_init(&aes_ctx);
        if (mbedtls_aes_setkey_dec(&aes_ctx, (const unsigned char *) key.data(), 128) != 0) {
            err = "mbedtls_aes_setkey_enc failed";
        }
        if (!err.length()) {
            for (size_t i = 0; i < result.length(); i += 16) {
                if (mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, (const unsigned char *) result.data() + i, (unsigned char *) result.data() + i) != 0) {
                    err = "mbedtls_aes_crypt_ecb failed";
                    break;
                }
            }
        }
        mbedtls_aes_free(&aes_ctx);
#else
        int p_len = cipher.length();
        int f_len = 0;

        EVP_CIPHER_CTX* de = EVP_CIPHER_CTX_new();
        EVP_CIPHER_CTX_init(de);
        if(EVP_DecryptInit_ex(de, EVP_aes_128_ecb(), NULL, (const unsigned char *) key.data(), NULL) != 1) {
            err = "EVP_DecryptInit_ex failed";
        }
        if(!err.length() && EVP_DecryptUpdate(de, (unsigned char *) result.data(), &p_len, (const unsigned char *) cipher.data(), cipher.length()) != 1) {
            err = "EVP_DecryptUpdate failed";
        }
        if(EVP_DecryptFinal_ex(de, (unsigned char *) result.data() + p_len, &f_len) != 1) {
            err = "EVP_DecryptFinal_ex failed";
        }
        EVP_CIPHER_CTX_free(de);
#endif

        p_len += f_len;
        if (!err.length()) {
#ifdef TUYACPP_USE_MBEDTLS
            // TODO: for some reason, even when using the mbedtls_cipher API, we cannot get the correct length, so we have to rely on the JSON format
            result.resize(result.find_last_of('}') + 1);
#else
            result.resize(p_len);
#endif
        } else {
            result.clear();
            LOGE() << "decrypt() failed: " << err << std::endl;
        }

        return result;
    }

    uint32_t mPrefix;
    uint32_t mSeqNo;
    uint32_t mCmd;
    uint32_t mRetCode;
    ordered_json mData;
};

} // namespace tuya
