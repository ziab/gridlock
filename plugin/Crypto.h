#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace Crypto
{

inline std::array<uint8_t, 20> sha1 (const void *data, size_t len) noexcept
{
    auto leftRotate = [] (uint32_t value, int bits) -> uint32_t
    { return (value << bits) | (value >> (32 - bits)); };

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    uint64_t bitLen = static_cast<uint64_t> (len) * 8;
    std::vector<uint8_t> msg (static_cast<const uint8_t *> (data),
                              static_cast<const uint8_t *> (data) + len);
    msg.push_back (0x80);
    while (msg.size () % 64 != 56)
        msg.push_back (0x00);
    for (int i = 7; i >= 0; --i)
        msg.push_back (static_cast<uint8_t> ((bitLen >> (i * 8)) & 0xFF));

    for (size_t offset = 0; offset < msg.size (); offset += 64)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (static_cast<uint32_t> (msg[offset + i * 4]) << 24)
                 | (static_cast<uint32_t> (msg[offset + i * 4 + 1]) << 16)
                 | (static_cast<uint32_t> (msg[offset + i * 4 + 2]) << 8)
                 | (static_cast<uint32_t> (msg[offset + i * 4 + 3]));
        for (int i = 16; i < 80; ++i)
            w[i] = leftRotate (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i)
        {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;            k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;            k = 0xCA62C1D6; }
            uint32_t temp = leftRotate (a, 5) + f + e + k + w[i];
            e = d; d = c; c = leftRotate (b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> digest;
    auto store = [&digest] (int off, uint32_t v)
    {
        digest[off]     = static_cast<uint8_t> ((v >> 24) & 0xFF);
        digest[off + 1] = static_cast<uint8_t> ((v >> 16) & 0xFF);
        digest[off + 2] = static_cast<uint8_t> ((v >> 8) & 0xFF);
        digest[off + 3] = static_cast<uint8_t> (v & 0xFF);
    };
    store (0, h0); store (4, h1); store (8, h2); store (12, h3); store (16, h4);
    return digest;
}

} // namespace Crypto
