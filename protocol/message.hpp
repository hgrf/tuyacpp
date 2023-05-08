#pragma once

#include <iostream>
#include <sstream>

#include <openssl/evp.h>

#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

#include "common.hpp"

namespace tuya {

class Message {
public:
    Message(uint32_t prefix, uint32_t seqNo, uint32_t cmd, const ordered_json& data) :
    mPrefix(prefix),
    mSeqNo(seqNo),
    mCmd(cmd),
    mData(data) {

    }

    operator std::string() const {
        std::ostringstream ss;
        ss << std::hex << "Message { prefix: 0x" << mPrefix << ", seqno: 0x" << mSeqNo  << ", cmd: 0x" << mCmd << " }";
        return ss.str();
    }

    const ordered_json& data() const {
        return mData;
    }

    uint32_t prefix() const {
        return mPrefix;
    }

    virtual std::string serialize(const std::string& key = DEFAULT_KEY, bool noRetCode = true) = 0;
    static std::unique_ptr<Message> deserialize(const std::string& raw, const std::string& key = DEFAULT_KEY, bool noRetCode = false);

protected:
    static std::string encrypt(const std::string& plain, const std::string& key) {
        std::string err;
        const char padNum = 16 - plain.length() % 16;
        std::string result = plain + std::string(padNum, padNum);
        int p_len = result.length(), f_len = 0;

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

        if(err.length()) {
            result.clear();
            std::cerr << "encrypt() failed: " << err << std::endl;
        }

        return result;
    }


    static std::string decrypt(const std::string& cipher, const std::string& key) {
        // TODO: padding operation should be symmetric with encrypt
        std::string err;
        std::string result = cipher;
        int p_len = cipher.length(), f_len = 0;

        EVP_CIPHER_CTX* de = EVP_CIPHER_CTX_new();
        EVP_CIPHER_CTX_init(de);
        if(EVP_DecryptInit_ex(de, EVP_aes_128_ecb(), NULL, (const unsigned char *) key.data(), NULL) != 1) {
            err = "EVP_DecryptInit_ex failed";
        }
        if(!err.length() && EVP_DecryptUpdate(de, (unsigned char *) result.data(), &p_len, (const unsigned char *) cipher.data(), cipher.length()) != 1) {
            err = "EVP_DecryptUpdate failed";
        }
        // if(EVP_DecryptFinal_ex(de, (unsigned char *) result.data() + p_len, &f_len) != 1) {
        //     err = "EVP_DecryptFinal_ex failed";
        // }
        EVP_CIPHER_CTX_free(de);

        if(!err.length()) {
            int padlen = result[p_len - 1];
            if (padlen < 1 || padlen > 16) {
                err = "Invalid padding length";
            } else {
                result.resize(p_len - padlen);
            }
        }

        if (err.length()) {
            result.clear();
            std::cerr << "decrypt() failed: " << err << std::endl;
        }

        return result;
    }

    uint32_t mPrefix;
    uint32_t mSeqNo;
    uint32_t mCmd;
    ordered_json mData;
};

} // namespace tuya
