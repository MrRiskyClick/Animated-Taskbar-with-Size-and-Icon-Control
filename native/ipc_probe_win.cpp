// STAGE 1e: standalone Windows mock IPC identity/challenge probe.
// No Windhawk calls, no Explorer hooks, no registry or taskbar changes.
// This is NOT a production service or recovery authority.
#ifndef _WIN32
#error This probe only builds on Windows.
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#include "native/ipc_protocol.hpp"
#include "native/ipc_timeout_win.hpp"
#include "src/core/diagnostic_ipc_win.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <cwchar>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using namespace atb_ipc;

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
    ~Handle() { if (h != INVALID_HANDLE_VALUE && h != nullptr) CloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const { return h; }
    bool valid() const { return h != INVALID_HANDLE_VALUE && h != nullptr; }
};

bool TokenUserSid(HANDLE token, std::vector<BYTE>& memory, PSID& sid) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (!size || size > 65536) return false;
    memory.resize(size);
    DWORD got = 0;
    if (!GetTokenInformation(token, TokenUser, memory.data(), size, &got)) return false;
    auto* user = reinterpret_cast<TOKEN_USER*>(memory.data());
    if (!IsValidSid(user->User.Sid)) return false;
    sid = user->User.Sid;
    return true;
}

bool SameUserAndSession(HANDLE pipe) {
    ULONG clientPid = 0, clientSession = 0;
    DWORD serverSession = 0;
    if (!GetNamedPipeClientProcessId(pipe, &clientPid) || clientPid == 0 ||
        !GetNamedPipeClientSessionId(pipe, &clientSession) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &serverSession) ||
        clientSession != serverSession) return false;
    Handle serverToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &serverToken.h)) return false;
    std::vector<BYTE> serverMemory, clientMemory;
    PSID serverSid = nullptr, clientSid = nullptr;
    if (!TokenUserSid(serverToken.get(), serverMemory, serverSid)) return false;
    // A client must have sent a HELLO first before impersonation can work.
    if (!ImpersonateNamedPipeClient(pipe)) return false;
    Handle clientToken;
    bool result = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &clientToken.h) &&
                  TokenUserSid(clientToken.get(), clientMemory, clientSid) &&
                  EqualSid(serverSid, clientSid);
    if (!RevertToSelf()) return false;
    return result;
}

// Client-side peer check: inspect the actual process behind the pipe handle.
// This diagnostic expects server and client to be the SAME test executable;
// the real watchdog will need an independently pinned/signed binary identity.
bool VerifyServerIdentity(HANDLE pipe, DWORD expectedPid) {
    ULONG serverPid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &serverPid) ||
        serverPid == 0 || serverPid != expectedPid) return false;
    Handle server(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, serverPid));
    if (!server.valid()) return false;
    DWORD serverSession = 0, ourSession = 0;
    if (!ProcessIdToSessionId(serverPid, &serverSession) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &ourSession) ||
        serverSession != ourSession) return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(server.get(), &exitCode) || exitCode != STILL_ACTIVE)
        return false;
    Handle serverToken, ourToken;
    if (!OpenProcessToken(server.get(), TOKEN_QUERY, &serverToken.h) ||
        !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ourToken.h)) return false;
    std::vector<BYTE> serverBytes, ourBytes;
    PSID serverSid = nullptr, ourSid = nullptr;
    if (!TokenUserSid(serverToken.get(), serverBytes, serverSid) ||
        !TokenUserSid(ourToken.get(), ourBytes, ourSid) ||
        !EqualSid(serverSid, ourSid)) return false;
    std::wstring serverImage(32768, L'\0');
    DWORD chars = static_cast<DWORD>(serverImage.size());
    if (!QueryFullProcessImageNameW(server.get(), 0, serverImage.data(), &chars) ||
        !chars) return false;
    serverImage.resize(chars);
    wchar_t ownImage[32768]{};
    DWORD ownLength = GetModuleFileNameW(nullptr, ownImage, 32768);
    return ownLength != 0 && ownLength < 32768 &&
           _wcsicmp(serverImage.c_str(), ownImage) == 0;
}

bool UserOnlySecurity(PSECURITY_DESCRIPTOR& descriptor) {
    descriptor = nullptr;
    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.h)) return false;
    std::vector<BYTE> sidBytes;
    PSID sid = nullptr;
    if (!TokenUserSid(token.get(), sidBytes, sid)) return false;
    LPWSTR sidString = nullptr;
    if (!ConvertSidToStringSidW(sid, &sidString)) return false;
    const std::wstring acl = L"D:P(A;;GA;;;SY)(A;;GA;;;" + std::wstring(sidString) + L")";
    LocalFree(sidString);
    return ConvertStringSecurityDescriptorToSecurityDescriptorW(
        acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr) != 0;
}

bool RandomBytes(BYTE* bytes, ULONG count) {
    return BCryptGenRandom(nullptr, bytes, count, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}

bool HmacSha256(const Secret& secret, const Challenge& challenge,
                const char* domain, Secret& mac) {
    // Separate server and client tags to prevent reflection.
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return false;
    DWORD size = 0, result = 0;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size),
                          sizeof(size), &result, 0) >= 0 && size > 0 && size <= 4096) {
        std::vector<BYTE> storage(size);
        if (BCryptCreateHash(alg, &hash, storage.data(), size,
                             const_cast<PUCHAR>(secret.data()),
                             static_cast<ULONG>(secret.size()), 0) >= 0) {
            ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(domain)),
                                static_cast<ULONG>(std::strlen(domain)), 0) >= 0 &&
                 BCryptHashData(hash, const_cast<PUCHAR>(challenge.data()),
                                static_cast<ULONG>(challenge.size()), 0) >= 0 &&
                 BCryptFinishHash(hash, mac.data(), static_cast<ULONG>(mac.size()), 0) >= 0;
        }
    }
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}


bool ReadSecret(Secret& secret) {
    std::string hex;
    if (!std::getline(std::cin, hex)) return false;
    return HexDecode(hex, secret);
}

std::wstring PipeName(const char* suffix) {
    std::string value(suffix);
    if (value.empty() || value.size() > 48) return {};
    for (char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return {};
    return L"\\\\.\\pipe\\ATB-MOCK-IPC-" + std::wstring(value.begin(), value.end());
}

// Diagnostic-only peer observation server. No secret, no authorization, no
// recovery control. Deliberately separate from the HMAC authentication mode.
int ServeReadOnly(const std::wstring& pipeName, bool badReply = false) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!UserOnlySecurity(descriptor)) return 2;
    SECURITY_ATTRIBUTES attrs{};
    attrs.nLength = sizeof(attrs);
    attrs.lpSecurityDescriptor = descriptor;
    attrs.bInheritHandle = FALSE;
    Handle pipe(CreateNamedPipeW(pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 512, 512, 0, &attrs));
    LocalFree(descriptor);
    if (!pipe.valid()) return 2;
    atb_transport::Deadline deadline;
    if (!atb_transport::Connect(pipe.get(), deadline)) return 3;
    std::array<std::uint8_t, 8> hello{};
    if (!atb_transport::ReadExact(pipe.get(), hello, deadline) ||
        hello != atb_readonly::kHello || !SameUserAndSession(pipe.get())) return 3;
    auto response = atb_readonly::kReply;
    if (badReply) response[0] ^= 0xff;
    return atb_transport::WriteExact(pipe.get(), response, deadline) ? 0 : 3;
}

int Serve(const std::wstring& pipeName, bool stallAfterHello = false) {
    Secret secret{};
    if (!ReadSecret(secret)) return 2;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!UserOnlySecurity(descriptor)) return 2;
    SECURITY_ATTRIBUTES attrs{};
    attrs.nLength = sizeof(attrs);
    attrs.lpSecurityDescriptor = descriptor;
    attrs.bInheritHandle = FALSE;
    Handle pipe(CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 512, 512, 0, &attrs));
    LocalFree(descriptor);
    if (!pipe.valid()) return 2;
    atb_transport::Deadline deadline;
    if (!atb_transport::Connect(pipe.get(), deadline)) return 2;
    std::array<std::uint8_t, kHelloBytes> hello{};
    if (!atb_transport::ReadExact(pipe.get(), hello, deadline) || hello != kHello ||
        !SameUserAndSession(pipe.get()))
        return 3;
    if (stallAfterHello) { Sleep(6000); return 3; }
    std::array<std::uint8_t, 32> nonce{};
    std::array<std::uint8_t, 16> trial{};
    if (!RandomBytes(nonce.data(), static_cast<ULONG>(nonce.size())) ||
        !RandomBytes(trial.data(), static_cast<ULONG>(trial.size()))) return 3;
    const Challenge challenge = MakeChallenge(nonce, trial, GetTickCount64());
    Secret serverTag{};
    if (!HmacSha256(secret, challenge, "ATB-STAGE1E-SERVER-v1", serverTag) ||
        !atb_transport::WriteExact(pipe.get(), challenge, deadline) ||
        !atb_transport::WriteExact(pipe.get(), serverTag, deadline)) return 3;
    Response response{};
    if (!atb_transport::ReadExact(pipe.get(), response, deadline) ||
        !ValidateChallenge(challenge, GetTickCount64())) return 3;
    Secret expected{};
    if (!HmacSha256(secret, challenge, "ATB-STAGE1E-CLIENT-v1", expected)) return 3;
    const bool accepted = CheckResponse(response, expected);
    constexpr std::array<std::uint8_t, 8> ok{{'A','T','B','O','K','0','0','1'}};
    constexpr std::array<std::uint8_t, 8> no{{'A','T','B','N','O','0','0','1'}};
    if (!atb_transport::WriteExact(pipe.get(), accepted ? ok : no, deadline)) return 3;
    // Do not call synchronous FlushFileBuffers: it can block on a hung peer.
    std::cout << (accepted ? "AUTHENTICATED-MOCK-ONLY\n" : "AUTH-REJECTED\n");
    return accepted ? 0 : 3;
}

int Client(const std::wstring& pipeName, bool captureProof, bool replayProof,
           DWORD expectedServerPid, bool stallAfterConnect = false) {
    Secret material{};
    if (!ReadSecret(material)) return 2;
    Secret replayTag{};
    if (replayProof && !ReadSecret(replayTag)) return 2;
    atb_transport::Deadline deadline;
    Handle pipe(atb_transport::OpenClient(pipeName, deadline));
    if (!pipe.valid()) return 2;
    if (!VerifyServerIdentity(pipe.get(), expectedServerPid)) return 3;
    if (stallAfterConnect) { Sleep(6000); return 3; }
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr) ||
        !atb_transport::WriteExact(pipe.get(), kHello, deadline)) return 3;
    Challenge challenge{};
    if (!atb_transport::ReadExact(pipe.get(), challenge, deadline) || !ValidateChallenge(challenge, GetTickCount64()))
        return 3;
    Secret receivedServerTag{}, expectedServerTag{};
    if (!atb_transport::ReadExact(pipe.get(), receivedServerTag, deadline) ||
        !HmacSha256(material, challenge, "ATB-STAGE1E-SERVER-v1", expectedServerTag) ||
        !FixedEqual(receivedServerTag.data(), expectedServerTag.data(), 32)) return 3;
    Secret proof{};
    if (replayProof) {
        proof = replayTag; // Old proof is invalid against this trial's nonce.
    } else if (!HmacSha256(material, challenge, "ATB-STAGE1E-CLIENT-v1", proof)) {
        return 3;
    }
    const auto response = MakeResponse(proof);
    if (!atb_transport::WriteExact(pipe.get(), response, deadline)) return 3;
    std::array<std::uint8_t, 8> acknowledgment{};
    if (!atb_transport::ReadExact(pipe.get(), acknowledgment, deadline)) return 3;
    const std::array<std::uint8_t, 8> ok{{'A','T','B','O','K','0','0','1'}};
    if (captureProof) {
        auto encoded = HexEncode(proof);
        // Test-only proof, not a secret and not an authorization for new trials.
        std::cout << "TEST_PROOF=" << std::string(encoded.data(), encoded.size()) << "\n";
    }
    return acknowledgment == ok ? 0 : 3;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: ipc_probe_win.exe server|server-stall|client|client-stall|client-capture|client-replay <pipe-suffix> [expected-server-pid]\n";
        return 2;
    }
    const std::wstring pipe = PipeName(argv[2]);
    if (pipe.empty()) return 2;
    const std::string mode(argv[1]);
    if (mode == "server" && argc == 3) return Serve(pipe);
    if (mode == "observer-server" && argc == 3) return ServeReadOnly(pipe);
    if (mode == "observer-server-bad" && argc == 3) return ServeReadOnly(pipe, true);
    if (mode == "server-stall" && argc == 3) return Serve(pipe, true);
    if (argc != 4) return 2;
    char* end = nullptr;
    unsigned long pid = std::strtoul(argv[3], &end, 10);
    if (!end || *end != '\0' || pid == 0 || pid > 0xffffffffUL) return 2;
    if (mode == "client") return Client(pipe, false, false, static_cast<DWORD>(pid));
    if (mode == "client-stall") return Client(pipe, false, false, static_cast<DWORD>(pid), true);
    if (mode == "client-capture") return Client(pipe, true, false, static_cast<DWORD>(pid));
    if (mode == "client-replay") return Client(pipe, false, true, static_cast<DWORD>(pid));
    return 2;
}
