// Stage 1h: standalone read-only binary identity policy host. No Windhawk
// injection, no registry writes, no taskbar modifications, no install step.
#include "native/watchdog_identity_win.hpp"
#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
std::wstring FromUtf8(const char* utf8) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        utf8, -1, nullptr, 0);
    if (size <= 1 || size >= 32768) return {};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1,
                             result.data(), size)) return {};
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}
bool PolicyFromArguments(const char* expectedHash, const char* signedFlag,
                         atb_identity::Policy& policy) {
    const std::string sig(signedFlag);
    if (sig != "0" && sig != "1") return false;
    if (!atb_identity::DecodePinnedDigest(expectedHash,
                                          policy.trustedFullFileHash)) return false;
    policy.requireWindowsSignature = sig == "1";
    return true;
}
void Print(bool ok) {
    std::cout << (ok ? "IDENTITY-OBSERVED\n" : "IDENTITY-REJECTED\n");
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string mode(argv[1]);
    if (mode == "file" && argc == 5) {
        atb_identity::Policy policy;
        if (!PolicyFromArguments(argv[3], argv[4], policy)) return 2;
        const bool ok = atb_identity::VerifyFile(FromUtf8(argv[2]), policy);
        Print(ok);
        return ok ? 0 : 3;
    }
    if ((mode == "self" || mode == "self-stale" || mode == "wrong-pid") &&
        argc == 4) {
        atb_identity::Policy policy;
        if (!PolicyFromArguments(argv[2], argv[3], policy)) return 2;
        atb_identity::ScopedFile self(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                                 FALSE, GetCurrentProcessId()));
        if (!self.valid()) return 3;
        std::uint64_t time = 0;
        if (!atb_identity::GetCreationTime(self.handle, time)) return 3;
        if (mode == "self-stale") ++time;
        DWORD pid = mode == "wrong-pid" ? 0 : GetCurrentProcessId();
        bool ok = atb_identity::VerifyProcess(pid, time, policy);
        Print(ok);
        return ok ? 0 : 3;
    }
    std::cerr << "Usage: binary_identity_probe_win.exe file <path> <sha256> 0|1\n"
              << "   or: binary_identity_probe_win.exe self|self-stale|wrong-pid <sha256> 0|1\n";
    return 2;
}
