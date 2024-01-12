#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pyhandler {

static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

static inline bool is_base64(uint8_t c) {
    return (std::isalnum(c) || (c == '+') || (c == '/'));
}

inline std::string base64_encode(const std::vector<uint8_t>& data) {
    size_t n = data.size();

    std::string ret;
    int i = 0;
    int j = 0;
    int k = 0;
    uint8_t c3[3];
    uint8_t c4[4];

    while (n--) {
        c3[i++] = data[k++];
        if (i == 3) {
            c4[0] = (c3[0] & 0xfc) >> 2;
            c4[1] = ((c3[0] & 0x03) << 4) + ((c3[1] & 0xf0) >> 4);
            c4[2] = ((c3[1] & 0x0f) << 2) + ((c3[2] & 0xc0) >> 6);
            c4[3] = c3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                ret += base64_chars[c4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            c3[j] = '\0';
        }

        c4[0] = (c3[0] & 0xfc) >> 2;
        c4[1] = ((c3[0] & 0x03) << 4) + ((c3[1] & 0xf0) >> 4);
        c4[2] = ((c3[1] & 0x0f) << 2) + ((c3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++) {
            ret += base64_chars[c4[j]];
        }

        for (j = i; j < 3; j++) {
            ret += '=';
        }
    }

    return ret;
}

inline std::vector<uint8_t> base64_decode(const std::string& encoded) {
    size_t in_len = encoded.size();
    int i = 0;
    int j = 0;
    int k = 0;
    uint8_t c3[3];
    uint8_t c4[4];
    std::vector<uint8_t> ret;

    while (in_len-- && (encoded[k] != '=') && is_base64(encoded[k])) {
        c4[i++] = encoded[k];
        k++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                c4[i] = base64_chars.find(c4[i]) & 0xff;
            }

            c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) + (c4[3]);

            for (i = 0; i < 3; i++) {
                ret.push_back(c3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++) {
            c4[j] = base64_chars.find(c4[j]) & 0xff;
        }

        c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);

        for (j = 0; j < i - 1; j++) {
            ret.push_back(c3[j]);
        }
    }

    return ret;
}

}  // namespace pyhandler
