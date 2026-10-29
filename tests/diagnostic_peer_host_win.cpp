// Standalone test host using exactly the read-only diagnostic client header
// compiled into the Windhawk prototype. This is NOT injected into Explorer.
#include "src/core/diagnostic_ipc_win.hpp"
#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const std::string suffix(argv[1]);
    char* tail = nullptr;
    const unsigned long parsed = std::strtoul(argv[2], &tail, 10);
    if (!tail || *tail != '\0' || parsed == 0 || parsed > 0xffffffffUL)
        return 2;
    const std::string bytes(argv[3]);
    if (bytes.empty() || bytes.size() > 32760) return 2;
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           bytes.c_str(), -1, nullptr, 0);
    if (needed < 1) return 2;
    std::wstring exe(static_cast<std::size_t>(needed), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                             bytes.c_str(), -1, exe.data(), needed))
        return 2;
    exe.resize(static_cast<std::size_t>(needed - 1));
    const std::wstring wsuffix(suffix.begin(), suffix.end());
    const bool observed = atb_readonly::Probe(wsuffix, static_cast<DWORD>(parsed), exe);
    std::cout << (observed ? "READ_ONLY_PEER_OK\n" : "READ_ONLY_PEER_REJECTED\n");
    return observed ? 0 : 3;
}
