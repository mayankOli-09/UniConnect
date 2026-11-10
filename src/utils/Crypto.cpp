#include "utils/Crypto.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace uniconnect {
namespace utils {

// --- Base64 ---
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Crypto::base64Encode(const unsigned char* bytes_to_encode, size_t in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
        while ((i++ < 3)) ret += '=';
    }

    return ret;
}

std::string Crypto::base64Encode(const std::string& input) {
    return base64Encode(reinterpret_cast<const unsigned char*>(input.data()), input.size());
}

std::string Crypto::base64Decode(const std::string& encoded_string) {
    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) char_array_4[j] = 0;
        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; j < i - 1; j++) ret += char_array_3[j];
    }

    return ret;
}

// --- SHA-1 Implementation (RFC 3174) ---
namespace {
    inline uint32_t rol(uint32_t value, size_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    inline uint32_t blk(const uint32_t block[16], size_t i) {
        return rol(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i & 15], 1);
    }

    inline void R0(const uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
        z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
        w = rol(w, 30);
    }
    inline void R1(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
        block[i & 15] = blk(block, i);
        z += ((w & (x ^ y)) ^ y) + block[i & 15] + 0x5a827999 + rol(v, 5);
        w = rol(w, 30);
    }
    inline void R2(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
        block[i & 15] = blk(block, i);
        z += (w ^ x ^ y) + block[i & 15] + 0x6ed9eba1 + rol(v, 5);
        w = rol(w, 30);
    }
    inline void R3(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
        block[i & 15] = blk(block, i);
        z += (((w | x) & y) | (w & x)) + block[i & 15] + 0x8f1bbcdc + rol(v, 5);
        w = rol(w, 30);
    }
    inline void R4(uint32_t block[16], uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i) {
        block[i & 15] = blk(block, i);
        z += (w ^ x ^ y) + block[i & 15] + 0xca62c1d6 + rol(v, 5);
        w = rol(w, 30);
    }
}

std::vector<uint8_t> Crypto::sha1(const std::string& input) {
    uint32_t digest[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};

    uint64_t total_bits = input.size() * 8;
    std::vector<uint8_t> data(input.begin(), input.end());
    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t block[16];
        for (int i = 0; i < 16; ++i) {
            block[i] = (static_cast<uint32_t>(data[chunk + i * 4]) << 24) |
                       (static_cast<uint32_t>(data[chunk + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(data[chunk + i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(data[chunk + i * 4 + 3]));
        }

        uint32_t a = digest[0];
        uint32_t b = digest[1];
        uint32_t c = digest[2];
        uint32_t d = digest[3];
        uint32_t e = digest[4];

        R0(block, a, b, c, d, e,  0); R0(block, e, a, b, c, d,  1); R0(block, d, e, a, b, c,  2); R0(block, c, d, e, a, b,  3);
        R0(block, b, c, d, e, a,  4); R0(block, a, b, c, d, e,  5); R0(block, e, a, b, c, d,  6); R0(block, d, e, a, b, c,  7);
        R0(block, c, d, e, a, b,  8); R0(block, b, c, d, e, a,  9); R0(block, a, b, c, d, e, 10); R0(block, e, a, b, c, d, 11);
        R0(block, d, e, a, b, c, 12); R0(block, c, d, e, a, b, 13); R0(block, b, c, d, e, a, 14); R0(block, a, b, c, d, e, 15);
        R1(block, e, a, b, c, d, 16); R1(block, d, e, a, b, c, 17); R1(block, c, d, e, a, b, 18); R1(block, b, c, d, e, a, 19);
        R2(block, a, b, c, d, e, 20); R2(block, e, a, b, c, d, 21); R2(block, d, e, a, b, c, 22); R2(block, c, d, e, a, b, 23);
        R2(block, b, c, d, e, a, 24); R2(block, a, b, c, d, e, 25); R2(block, e, a, b, c, d, 26); R2(block, d, e, a, b, c, 27);
        R2(block, c, d, e, a, b, 28); R2(block, b, c, d, e, a, 29); R2(block, a, b, c, d, e, 30); R2(block, e, a, b, c, d, 31);
        R2(block, d, e, a, b, c, 32); R2(block, c, d, e, a, b, 33); R2(block, b, c, d, e, a, 34); R2(block, a, b, c, d, e, 35);
        R2(block, e, a, b, c, d, 36); R2(block, d, e, a, b, c, 37); R2(block, c, d, e, a, b, 38); R2(block, b, c, d, e, a, 39);
        R3(block, a, b, c, d, e, 40); R3(block, e, a, b, c, d, 41); R3(block, d, e, a, b, c, 42); R3(block, c, d, e, a, b, 43);
        R3(block, b, c, d, e, a, 44); R3(block, a, b, c, d, e, 45); R3(block, e, a, b, c, d, 46); R3(block, d, e, a, b, c, 47);
        R3(block, c, d, e, a, b, 48); R3(block, b, c, d, e, a, 49); R3(block, a, b, c, d, e, 50); R3(block, e, a, b, c, d, 51);
        R3(block, d, e, a, b, c, 52); R3(block, c, d, e, a, b, 53); R3(block, b, c, d, e, a, 54); R3(block, a, b, c, d, e, 55);
        R3(block, e, a, b, c, d, 56); R3(block, d, e, a, b, c, 57); R3(block, c, d, e, a, b, 58); R3(block, b, c, d, e, a, 59);
        R4(block, a, b, c, d, e, 60); R4(block, e, a, b, c, d, 61); R4(block, d, e, a, b, c, 62); R4(block, c, d, e, a, b, 63);
        R4(block, b, c, d, e, a, 64); R4(block, a, b, c, d, e, 65); R4(block, e, a, b, c, d, 66); R4(block, d, e, a, b, c, 67);
        R4(block, c, d, e, a, b, 68); R4(block, b, c, d, e, a, 69); R4(block, a, b, c, d, e, 70); R4(block, e, a, b, c, d, 71);
        R4(block, d, e, a, b, c, 72); R4(block, c, d, e, a, b, 73); R4(block, b, c, d, e, a, 74); R4(block, a, b, c, d, e, 75);
        R4(block, e, a, b, c, d, 76); R4(block, d, e, a, b, c, 77); R4(block, c, d, e, a, b, 78); R4(block, b, c, d, e, a, 79);

        digest[0] += a; digest[1] += b; digest[2] += c; digest[3] += d; digest[4] += e;
    }

    std::vector<uint8_t> result(20);
    for (int i = 0; i < 5; ++i) {
        result[i * 4] = static_cast<uint8_t>((digest[i] >> 24) & 0xFF);
        result[i * 4 + 1] = static_cast<uint8_t>((digest[i] >> 16) & 0xFF);
        result[i * 4 + 2] = static_cast<uint8_t>((digest[i] >> 8) & 0xFF);
        result[i * 4 + 3] = static_cast<uint8_t>(digest[i] & 0xFF);
    }
    return result;
}

std::string Crypto::sha1Hex(const std::string& input) {
    auto bytes = sha1(input);
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

// --- SHA-256 Implementation (FIPS 180-4) ---
namespace {
    inline uint32_t ror(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    inline uint32_t sig0(uint32_t x) { return ror(x, 2) ^ ror(x, 13) ^ ror(x, 22); }
    inline uint32_t sig1(uint32_t x) { return ror(x, 6) ^ ror(x, 11) ^ ror(x, 25); }
    inline uint32_t gam0(uint32_t x) { return ror(x, 7) ^ ror(x, 18) ^ (x >> 3); }
    inline uint32_t gam1(uint32_t x) { return ror(x, 17) ^ ror(x, 19) ^ (x >> 10); }

    static const uint32_t K256[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
}

std::vector<uint8_t> Crypto::sha256(const std::string& input) {
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint64_t total_bits = input.size() * 8;
    std::vector<uint8_t> data(input.begin(), input.end());
    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
        uint32_t W[64];
        for (int i = 0; i < 16; ++i) {
            W[i] = (static_cast<uint32_t>(data[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(data[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(data[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(data[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            W[i] = gam1(W[i - 2]) + W[i - 7] + gam0(W[i - 15]) + W[i - 16];
        }

        uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
        uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t T1 = h + sig1(e) + ch(e, f, g) + K256[i] + W[i];
            uint32_t T2 = sig0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + T1;
            d = c; c = b; b = a; a = T1 + T2;
        }

        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    std::vector<uint8_t> result(32);
    for (int i = 0; i < 8; ++i) {
        result[i * 4] = static_cast<uint8_t>((H[i] >> 24) & 0xFF);
        result[i * 4 + 1] = static_cast<uint8_t>((H[i] >> 16) & 0xFF);
        result[i * 4 + 2] = static_cast<uint8_t>((H[i] >> 8) & 0xFF);
        result[i * 4 + 3] = static_cast<uint8_t>(H[i] & 0xFF);
    }
    return result;
}

std::string Crypto::sha256Hex(const std::string& input) {
    auto bytes = sha256(input);
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

// --- HMAC-SHA256 Implementation (RFC 2104) ---
std::vector<uint8_t> Crypto::hmacSha256(const std::string& key_str, const std::string& data) {
    const size_t block_size = 64;
    std::vector<uint8_t> key(key_str.begin(), key_str.end());

    if (key.size() > block_size) {
        key = sha256(std::string(key.begin(), key.end()));
    }
    if (key.size() < block_size) {
        key.resize(block_size, 0x00);
    }

    std::vector<uint8_t> o_key_pad(block_size);
    std::vector<uint8_t> i_key_pad(block_size);
    for (size_t i = 0; i < block_size; ++i) {
        o_key_pad[i] = key[i] ^ 0x5c;
        i_key_pad[i] = key[i] ^ 0x36;
    }

    std::string inner_data(i_key_pad.begin(), i_key_pad.end());
    inner_data.append(data);
    auto inner_hash = sha256(inner_data);

    std::string outer_data(o_key_pad.begin(), o_key_pad.end());
    outer_data.append(inner_hash.begin(), inner_hash.end());
    return sha256(outer_data);
}

std::string Crypto::hmacSha256Hex(const std::string& key, const std::string& data) {
    auto bytes = hmacSha256(key, data);
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

std::string Crypto::hashPassword(const std::string& password, const std::string& salt) {
    return sha256Hex(salt + "$" + password + "$" + salt);
}

bool Crypto::verifyPassword(const std::string& password, const std::string& hash, const std::string& salt) {
    return hashPassword(password, salt) == hash;
}

// RFC 6455 Sec-WebSocket-Accept key calculation
std::string Crypto::generateWebSocketAcceptKey(const std::string& clientKey) {
    static const std::string ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string concatenated = clientKey + ws_guid;
    auto sha1_bytes = sha1(concatenated);
    return base64Encode(sha1_bytes.data(), sha1_bytes.size());
}

std::string Crypto::createJWT(const std::string& payloadJson, const std::string& secret) {
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string b64Header = base64Encode(header);
    std::string b64Payload = base64Encode(payloadJson);
    std::string dataToSign = b64Header + "." + b64Payload;
    auto sigBytes = hmacSha256(secret, dataToSign);
    std::string b64Sig = base64Encode(sigBytes.data(), sigBytes.size());
    return dataToSign + "." + b64Sig;
}

bool Crypto::verifyJWT(const std::string& token, const std::string& secret, std::string& outPayload) {
    auto dot1 = token.find('.');
    if (dot1 == std::string::npos) return false;
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return false;

    std::string b64Header = token.substr(0, dot1);
    std::string b64Payload = token.substr(dot1 + 1, dot2 - (dot1 + 1));
    std::string b64Sig = token.substr(dot2 + 1);

    std::string dataToSign = b64Header + "." + b64Payload;
    auto expectedSigBytes = hmacSha256(secret, dataToSign);
    std::string expectedSig = base64Encode(expectedSigBytes.data(), expectedSigBytes.size());

    if (b64Sig != expectedSig) {
        return false;
    }

    outPayload = base64Decode(b64Payload);
    return true;
}

} // namespace utils
} // namespace uniconnect
