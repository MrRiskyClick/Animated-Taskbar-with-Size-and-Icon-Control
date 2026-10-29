// Stage 1g: read-only Windows named-pipe peer DIAGNOSTIC for Windhawk.
// This is not cryptographic authentication, a recovery lease or authorization
// to change the Windows shell. It never writes taskbar/registry/XAML state.
#pragma once
#ifndef _WIN32
#error Windows-only read-only diagnostic
#endif
#include <windows.h>
#include <array>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>
#include "../../native/ipc_timeout_win.hpp"

namespace atb_readonly {
constexpr std::array<std::uint8_t, 8> kHello{{'A','T','B','D','I','A','G','1'}};
constexpr std::array<std::uint8_t, 8> kReply{{'A','T','B','R','D','Y','0','1'}};

struct ScopedHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle(h) {}
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ~ScopedHandle() {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr) CloseHandle(handle);
    }
    bool valid() const { return handle != INVALID_HANDLE_VALUE && handle != nullptr; }
};

inline bool UserSid(HANDLE token, std::vector<BYTE>& buffer, PSID& sid) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size < sizeof(TOKEN_USER) || size > 65536) return false;
    buffer.resize(size);
    DWORD got = 0;
    if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &got) ||
        got < sizeof(TOKEN_USER)) return false;
    sid = reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid;
    return IsValidSid(sid) != FALSE;
}

inline bool ValidSuffix(const std::wstring& suffix) {
    if (suffix.empty() || suffix.size() > 48) return false;
    for (wchar_t c : suffix) {
        if (!((c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-'))
            return false;
    }
    return true;
}

inline bool CheckServerProcess(HANDLE pipe, DWORD expectedPid,
                               const std::wstring& expectedImage) {
    if (!expectedPid || expectedImage.empty() || expectedImage.size() >= 32768)
        return false;
    ULONG realPid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &realPid) ||
        realPid != expectedPid) return false;
    ScopedHandle peer(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, realPid));
    if (!peer.valid()) return false;
    DWORD serverSession = 0, clientSession = 0, status = 0;
    if (!GetExitCodeProcess(peer.handle, &status) || status != STILL_ACTIVE ||
        !ProcessIdToSessionId(realPid, &serverSession) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &clientSession) ||
        serverSession != clientSession) return false;
    ScopedHandle peerToken, ourToken;
    if (!OpenProcessToken(peer.handle, TOKEN_QUERY, &peerToken.handle) ||
        !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ourToken.handle))
        return false;
    std::vector<BYTE> peerMemory, ourMemory;
    PSID peerSid = nullptr, ourSid = nullptr;
    if (!UserSid(peerToken.handle, peerMemory, peerSid) ||
        !UserSid(ourToken.handle, ourMemory, ourSid) ||
        !EqualSid(peerSid, ourSid)) return false;
    std::wstring actualImage(32768, L'\0');
    DWORD size = static_cast<DWORD>(actualImage.size());
    if (!QueryFullProcessImageNameW(peer.handle, 0, actualImage.data(), &size) ||
        size == 0 || size >= 32768) return false;
    actualImage.resize(size);
    // A writable path is NOT a cryptographic binary identity. A subsequent
    // production build must verify publisher/signature/file identity as well.
    return CompareStringOrdinal(actualImage.c_str(), -1, expectedImage.c_str(),
                                -1, TRUE) == CSTR_EQUAL;
}

inline bool Probe(const std::wstring& suffix, DWORD expectedPid,
                  const std::wstring& expectedExe) {
    if (!ValidSuffix(suffix) || !expectedPid || expectedExe.empty())
        return false;
    const std::wstring pipeName = L"\\\\.\\pipe\\ATB-MOCK-IPC-" + suffix;
    atb_transport::Deadline deadline;
    ScopedHandle pipe(atb_transport::OpenClient(pipeName, deadline));
    if (!pipe.valid()) return false;
    if (!CheckServerProcess(pipe.handle, expectedPid, expectedExe)) return false;
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.handle, &mode, nullptr, nullptr))
        return false;
    if (!atb_transport::WriteExact(pipe.handle, kHello, deadline)) return false;
    std::array<std::uint8_t, 8> reply{};
    return atb_transport::ReadExact(pipe.handle, reply, deadline) && reply == kReply;
}
} // namespace atb_readonly
