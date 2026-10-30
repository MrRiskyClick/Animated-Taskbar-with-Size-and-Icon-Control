// Original Stage-1e mock IPC wire contract. NO Win32 API or shell effects.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace atb_ipc {
constexpr std::size_t kSecretBytes = 32;
constexpr std::size_t kNonceBytes = 32;
constexpr std::size_t kChallengeBytes = 64;
constexpr std::size_t kResponseBytes = 40;
constexpr std::size_t kHelloBytes = 8;
constexpr std::uint64_t kMaxChallengeAgeMs = 5000;
constexpr std::array<std::uint8_t, 8> kHello{{'A','T','B','H','E','L','0','1'}};
constexpr std::array<std::uint8_t, 8> kChallengeMagic{{'A','T','B','C','H','A','0','1'}};
constexpr std::array<std::uint8_t, 8> kResponseMagic{{'A','T','B','R','S','P','0','1'}};
using Secret = std::array<std::uint8_t, kSecretBytes>;
using Challenge = std::array<std::uint8_t, kChallengeBytes>;
using Response = std::array<std::uint8_t, kResponseBytes>;

inline bool HexDecode(std::string_view src, Secret& out) {
    if (src.size() != 64) return false;
    Secret decoded{};
    for (std::size_t i = 0; i < 32; ++i) {
        auto half = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            return -1;
        };
        const int hi = half(src[i * 2]), lo = half(src[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        decoded[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    out = decoded;
    return true;
}

inline std::array<char, 64> HexEncode(const Secret& bytes) {
    constexpr char hex[] = "0123456789abcdef";
    std::array<char, 64> result{};
    for (std::size_t i = 0; i < 32; ++i) {
        result[i * 2] = hex[bytes[i] >> 4];
        result[i * 2 + 1] = hex[bytes[i] & 15];
    }
    return result;
}

inline bool FixedEqual(const std::uint8_t* a, const std::uint8_t* b, std::size_t n) {
    unsigned diff = 0;
    for (std::size_t i = 0; i < n; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

inline bool ValidateChallenge(const Challenge& c, std::uint64_t nowMs) {
    if (!FixedEqual(c.data(), kChallengeMagic.data(), 8)) return false;
    bool nonceNonzero = false, trialNonzero = false;
    for (std::size_t i = 8; i < 40; ++i) nonceNonzero |= c[i] != 0;
    for (std::size_t i = 40; i < 56; ++i) trialNonzero |= c[i] != 0;
    std::uint64_t timestamp = 0;
    for (std::size_t i = 0; i < 8; ++i) timestamp |= static_cast<std::uint64_t>(c[56 + i]) << (8 * i);
    return nonceNonzero && trialNonzero && timestamp != 0 &&
           nowMs >= timestamp && nowMs - timestamp <= kMaxChallengeAgeMs;
}

inline Challenge MakeChallenge(const std::array<std::uint8_t, 32>& nonce,
                               const std::array<std::uint8_t, 16>& trial,
                               std::uint64_t nowMs) {
    Challenge out{};
    for (std::size_t i = 0; i < 8; ++i) out[i] = kChallengeMagic[i];
    for (std::size_t i = 0; i < 32; ++i) out[8 + i] = nonce[i];
    for (std::size_t i = 0; i < 16; ++i) out[40 + i] = trial[i];
    for (std::size_t i = 0; i < 8; ++i)
        out[56 + i] = static_cast<std::uint8_t>((nowMs >> (8 * i)) & 0xff);
    return out;
}

inline Response MakeResponse(const Secret& mac) {
    Response out{};
    for (std::size_t i = 0; i < 8; ++i) out[i] = kResponseMagic[i];
    for (std::size_t i = 0; i < 32; ++i) out[8 + i] = mac[i];
    return out;
}

inline bool CheckResponse(const Response& response, const Secret& expected) {
    return FixedEqual(response.data(), kResponseMagic.data(), 8) &&
           FixedEqual(response.data() + 8, expected.data(), 32);
}
}  // namespace atb_ipc
