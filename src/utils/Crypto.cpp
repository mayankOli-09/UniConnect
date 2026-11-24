#include "utils/Crypto.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstring>
#include <stdexcept>

#if defined(_MSC_VER)
#pragma comment(lib, "bcrypt.lib")
#endif

namespace uniconnect {
namespace utils {
namespace crypto {

namespace {

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

class Sha256 {
public:
    Sha256() { reset(); }
    void reset() {
        h_[0] = 0x6a09e667; h_[1] = 0xbb67ae85; h_[2] = 0x3c6ef372; h_[3] = 0xa54ff53a;
        h_[4] = 0x510e527f; h_[5] = 0x9b05688c; h_[6] = 0x1f83d9ab; h_[7] = 0x5be0cd19;
        total_ = 0; len_ = 0;
    }
    void update(const uint8_t* p, size_t n) {
        total_ += n;
        while (n > 0) {
            const size_t take = (64 - len_ < n) ? 64 - len_ : n;
            std::memcpy(buf_ + len_, p, take);
            len_ += take; p += take; n -= take;
            if (len_ == 64) { compress(buf_); len_ = 0; }
        }
    }
    void final(uint8_t out[32]) {
        const uint64_t bits = total_ * 8;
        const uint8_t pad = 0x80;
        update(&pad, 1);
        const uint8_t zero = 0;
        while (len_ != 56) update(&zero, 1);
        uint8_t lenBytes[8];
        for (int i = 0; i < 8; ++i) lenBytes[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
        update(lenBytes, 8);
        for (int i = 0; i < 8; ++i) {
            out[i * 4] = static_cast<uint8_t>(h_[i] >> 24);
            out[i * 4 + 1] = static_cast<uint8_t>(h_[i] >> 16);
            out[i * 4 + 2] = static_cast<uint8_t>(h_[i] >> 8);
            out[i * 4 + 3] = static_cast<uint8_t>(h_[i]);
        }
    }

private:
    void compress(const uint8_t* b) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(b[i * 4]) << 24) | (uint32_t(b[i * 4 + 1]) << 16) | (uint32_t(b[i * 4 + 2]) << 8) | b[i * 4 + 3];
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h_[0], bb = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = h + S1 + ch + K[i] + w[i];
            const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t mj = (a & bb) ^ (a & c) ^ (bb & c);
            const uint32_t t2 = S0 + mj;
            h = g; g = f; f = e; e = d + t1; d = c; c = bb; bb = a; a = t1 + t2;
        }
        h_[0] += a; h_[1] += bb; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }
    uint32_t h_[8];
    uint8_t buf_[64];
    uint64_t total_;
    size_t len_;
};

inline const uint8_t* u8(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

// HMAC with the two pad blocks pre-absorbed, so each call costs two compressions for short messages.
class HmacKey {
public:
    explicit HmacKey(const std::string& key) {
        std::string k = key;
        if (k.size() > 64) k = sha256(k);
        uint8_t ipad[64], opad[64];
        std::memset(ipad, 0x36, 64);
        std::memset(opad, 0x5c, 64);
        for (size_t i = 0; i < k.size(); ++i) { ipad[i] ^= static_cast<uint8_t>(k[i]); opad[i] ^= static_cast<uint8_t>(k[i]); }
        inner_.update(ipad, 64);
        outer_.update(opad, 64);
    }
    void mac(const uint8_t* data, size_t n, uint8_t out[32]) const {
        Sha256 in = inner_;
        in.update(data, n);
        uint8_t ih[32];
        in.final(ih);
        Sha256 o = outer_;
        o.update(ih, 32);
        o.final(out);
    }
private:
    Sha256 inner_, outer_;
};

} // namespace

std::string sha1(const std::string& data) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::string msg = data;
    const uint64_t bits = static_cast<uint64_t>(data.size()) * 8;
    msg += static_cast<char>(0x80);
    while (msg.size() % 64 != 56) msg += '\0';
    for (int i = 7; i >= 0; --i) msg += static_cast<char>((bits >> (8 * i)) & 0xFF);
    auto rotl = [](uint32_t x, unsigned n) { return (x << n) | (x >> (32 - n)); };
    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            const uint8_t* b = u8(msg) + off + static_cast<size_t>(i) * 4;
            w[i] = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
        }
        for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const uint32_t t = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::string out;
    for (uint32_t v : h) for (int i = 3; i >= 0; --i) out += static_cast<char>((v >> (8 * i)) & 0xFF);
    return out;
}

std::string sha256(const std::string& data) {
    Sha256 s;
    s.update(u8(data), data.size());
    uint8_t out[32];
    s.final(out);
    return std::string(reinterpret_cast<char*>(out), 32);
}

std::string hmacSha256(const std::string& key, const std::string& data) {
    HmacKey k(key);
    uint8_t out[32];
    k.mac(u8(data), data.size(), out);
    return std::string(reinterpret_cast<char*>(out), 32);
}

std::string pbkdf2Sha256(const std::string& password, const std::string& salt, unsigned iterations, size_t dkLen) {
    const HmacKey key(password);
    std::string result;
    for (uint32_t block = 1; result.size() < dkLen; ++block) {
        std::string first = salt;
        first += static_cast<char>(block >> 24);
        first += static_cast<char>(block >> 16);
        first += static_cast<char>(block >> 8);
        first += static_cast<char>(block);
        uint8_t u[32], t[32];
        key.mac(u8(first), first.size(), u);
        std::memcpy(t, u, 32);
        for (unsigned i = 1; i < iterations; ++i) {
            uint8_t next[32];
            key.mac(u, 32, next);
            std::memcpy(u, next, 32);
            for (int j = 0; j < 32; ++j) t[j] ^= u[j];
        }
        result.append(reinterpret_cast<char*>(t), 32);
    }
    result.resize(dkLen);
    return result;
}

std::string toHex(const std::string& bytes) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char c : bytes) { out += d[c >> 4]; out += d[c & 15]; }
    return out;
}

bool fromHex(const std::string& hex, std::string& out) {
    if (hex.size() % 2 != 0) return false;
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string r;
    r.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = v(hex[i]), lo = v(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        r += static_cast<char>(hi * 16 + lo);
    }
    out = std::move(r);
    return true;
}

std::string base64Encode(const std::string& in) {
    std::string out = base64UrlEncode(in);
    for (auto& c : out) { if (c == '-') c = '+'; else if (c == '_') c = '/'; }
    while (out.size() % 4 != 0) out += '=';
    return out;
}

std::string base64UrlEncode(const std::string& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    size_t i = 0;
    while (i + 2 < in.size()) {
        const uint32_t n = (uint32_t(uint8_t(in[i])) << 16) | (uint32_t(uint8_t(in[i + 1])) << 8) | uint8_t(in[i + 2]);
        out += t[(n >> 18) & 63]; out += t[(n >> 12) & 63]; out += t[(n >> 6) & 63]; out += t[n & 63];
        i += 3;
    }
    if (i + 1 == in.size()) {
        const uint32_t n = uint32_t(uint8_t(in[i])) << 16;
        out += t[(n >> 18) & 63]; out += t[(n >> 12) & 63];
    } else if (i + 2 == in.size()) {
        const uint32_t n = (uint32_t(uint8_t(in[i])) << 16) | (uint32_t(uint8_t(in[i + 1])) << 8);
        out += t[(n >> 18) & 63]; out += t[(n >> 12) & 63]; out += t[(n >> 6) & 63];
    }
    return out;
}

bool base64UrlDecode(const std::string& in, std::string& out) {
    auto v = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-') return 62;
        if (c == '_') return 63;
        return -1;
    };
    if (in.size() % 4 == 1) return false;
    std::string r;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : in) {
        const int x = v(c);
        if (x < 0) return false;
        acc = (acc << 6) | static_cast<uint32_t>(x);
        bits += 6;
        if (bits >= 8) { bits -= 8; r += static_cast<char>((acc >> bits) & 0xFF); }
    }
    out = std::move(r);
    return true;
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

std::string randomBytes(size_t n) {
    std::string out(n, '\0');
    const LONG st = BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&out[0]), static_cast<ULONG>(n),
                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (st < 0) throw std::runtime_error("BCryptGenRandom failed");
    return out;
}

} // namespace crypto
} // namespace utils
} // namespace uniconnect
