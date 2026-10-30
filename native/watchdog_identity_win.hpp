// Stage 1h: Windows executable IDENTITY EVIDENCE, not runtime attestation.
// This file has no shell, Explorer, taskbar, registry or Windhawk writes.
//
// Production rule: both an independently pinned full-file SHA256 and a
// successfully verified Authenticode signature are mandatory. Unsigned mode
// below is for UNTRUSTED GitHub-hosted test executables only and MUST NOT
// be used as a production authorization mechanism.
#pragma once
#ifndef _WIN32
#error This verifier requires Windows.
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <Softpub.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace atb_identity {
using Digest = std::array<std::uint8_t, 32>;
constexpr DWORD kHashBufferSize = 65536;

struct ScopedFile {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit ScopedFile(HANDLE value = INVALID_HANDLE_VALUE) : handle(value) {}
    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;
    ~ScopedFile() {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
            CloseHandle(handle);
    }
    bool valid() const {
        return handle != INVALID_HANDLE_VALUE && handle != nullptr;
    }
};

inline int HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

inline bool DecodePinnedDigest(const std::string& text, Digest& out) {
    if (text.size() != 64) return false;
    Digest decoded{};
    for (std::size_t i = 0; i < decoded.size(); ++i) {
        const int hi = HexNibble(text[2*i]), lo = HexNibble(text[2*i+1]);
        if (hi < 0 || lo < 0) return false;
        decoded[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    out = decoded;
    return true;
}

inline bool FixedDigestEqual(const Digest& lhs, const Digest& rhs) {
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < lhs.size(); ++i) diff |= lhs[i] ^ rhs[i];
    return diff == 0;
}

inline bool HashFile(HANDLE file, Digest& result) {
    if (SetFilePointer(file, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER &&
        GetLastError() != NO_ERROR) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    bool ok = false;
    DWORD objectBytes = 0, propertyBytes = 0, hashBytes = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectBytes),
                          sizeof(objectBytes), &propertyBytes, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                          reinterpret_cast<PUCHAR>(&hashBytes),
                          sizeof(hashBytes), &propertyBytes, 0) >= 0 &&
        hashBytes == result.size() && objectBytes > 0 && objectBytes < 1<<20) {
        std::vector<BYTE> hashState(objectBytes);
        if (BCryptCreateHash(algorithm, &hash, hashState.data(), objectBytes,
                             nullptr, 0, 0) >= 0) {
            std::vector<BYTE> chunk(kHashBufferSize);
            bool failed = false;
            while (true) {
                DWORD count = 0;
                if (!ReadFile(file, chunk.data(), kHashBufferSize, &count, nullptr)) {
                    failed = true;
                    break;
                }
                if (count == 0) break;
                if (BCryptHashData(hash, chunk.data(), count, 0) < 0) {
                    failed = true;
                    break;
                }
            }
            if (!failed)
                ok = BCryptFinishHash(hash, result.data(),
                                      static_cast<ULONG>(result.size()), 0) >= 0;
        }
    }
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

// Authenticode trust means Windows accepts some signing chain; the full-file
// SHA256 pin also must match. An arbitrary signed app must NOT be allowed.
inline bool AuthenticodeValid(const std::wstring& path) {
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = path.c_str();
    WINTRUST_DATA trust{};
    trust.cbStruct = sizeof(trust);
    trust.dwUIChoice = WTD_UI_NONE;
    trust.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trust.dwUnionChoice = WTD_CHOICE_FILE;
    trust.pFile = &file;
    trust.dwStateAction = WTD_STATEACTION_VERIFY;
    // No network retrieval from a thread in Explorer. Missing cached
    // revocation information is a fail-closed result.
    trust.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    trust.dwUIContext = WTD_UICONTEXT_EXECUTE;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG status = WinVerifyTrust(nullptr, &action, &trust);
    trust.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &trust);
    // DO NOT use SUCCEEDED(status): WinVerifyTrust can return positive errors.
    return status == ERROR_SUCCESS;
}

struct Policy {
    Digest trustedFullFileHash{};
    bool requireWindowsSignature = true;
};

// For a trusted release, Policy.trustedFullFileHash must be compiled from
// independently reviewed signed release artifacts, NOT populated from an
// editable Windhawk setting or an unauthenticated peer message.
inline bool VerifyFile(const std::wstring& path, const Policy& policy) {
    if (path.empty() || path.size() >= 32768) return false;
    ScopedFile file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                nullptr));
    if (!file.valid()) return false; // deny writable/deletable while verifying
    DWORD type = GetFileType(file.handle);
    if (type != FILE_TYPE_DISK) return false;
    Digest actual{};
    if (!HashFile(file.handle, actual) ||
        !FixedDigestEqual(actual, policy.trustedFullFileHash)) return false;
    if (policy.requireWindowsSignature && !AuthenticodeValid(path))
        return false;
    // Not a guarantee that all code *already loaded in the process* matches
    // this on-disk file. Separate runtime integrity checks remain required.
    return true;
}

inline std::uint64_t FiletimeValue(const FILETIME& value) {
    ULARGE_INTEGER converted{};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

inline bool GetCreationTime(HANDLE process, std::uint64_t& creation) {
    FILETIME started{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process, &started, &exited, &kernel, &user)) return false;
    creation = FiletimeValue(started);
    return creation != 0;
}

inline bool VerifyProcess(DWORD expectedPid, std::uint64_t expectedCreationTime,
                          const Policy& policy) {
    if (!expectedPid || !expectedCreationTime) return false;
    ScopedFile process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, expectedPid));
    if (!process.valid()) return false;
    DWORD status = 0;
    std::uint64_t actualCreationTime = 0;
    if (!GetExitCodeProcess(process.handle, &status) || status != STILL_ACTIVE ||
        !GetCreationTime(process.handle, actualCreationTime) ||
        actualCreationTime != expectedCreationTime) return false;
    std::wstring path(32768, L'\0');
    DWORD count = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.handle, 0, path.data(), &count) ||
        count == 0 || count >= path.size()) return false;
    path.resize(count);
    if (!VerifyFile(path, policy)) return false;
    std::uint64_t finalTime = 0;
    return GetExitCodeProcess(process.handle, &status) &&
           status == STILL_ACTIVE &&
           GetCreationTime(process.handle, finalTime) &&
           finalTime == actualCreationTime;
}
} // namespace atb_identity
