// ==WindhawkMod==
// @id animated-taskbar-size-icon-control
// @name Animated Taskbar (Read-Only Lease Diagnostic)
// @description Diagnostic only: taskbar geometry and watchdog heartbeat observation; no shell modifications
// @version 0.0.2
// @author Animated Taskbar project contributors
// @include explorer.exe
// @architecture x86-64
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- diagnosticLeaseFile: ""
  $name: Diagnostic lease file (optional)
  $description: Read-only path to a mock watchdog lease.v1 file. Leave blank for no observation.
- diagnosticTrialId: ""
  $name: Expected trial ID (32 lowercase hexadecimal digits)
  $description: Must match the mock lease exactly. This is NOT permission to modify Windows.
*/
// ==/WindhawkModSettings==

// ==WindhawkModReadme==
/*
# Stage 1b read-only Windhawk diagnostic
This source **never installs hooks, changes taskbar height, system tray, XAML,
work area, registry values, or other Windhawk mod settings**.
It can monitor a user-specified `lease.v1` file emitted by the mock watchdog
and log a fail-closed diagnostic result, but the file is NOT authentication.
The session stops at 120 seconds even if the file still looks valid.

Windhawk mods are single-file; use scripts/bundle_windhawk.py to embed our
original src/core/lease_v1.hpp before compiling this prototype in Windhawk.
Do not install active taskbar-changing builds until an independent durable
rollback, authenticated IPC and Explorer failure handling are proven.
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <memory>
#include <string>
#include <utility>

#include "core/lease_v1.hpp" // Inlined by scripts/bundle_windhawk.py

namespace {
HANDLE g_stopEvent = nullptr;
HANDLE g_timerThread = nullptr;
volatile LONG g_diagnosticInactive = 0;
std::wstring g_leaseFile;
std::unique_ptr<animated_taskbar::LeaseObserver> g_observer;

std::uint64_t NowUtcMilliseconds() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    constexpr std::uint64_t kUnixEpoch100ns = 116444736000000000ULL;
    return (value.QuadPart - kUnixEpoch100ns) / 10000ULL;
}

bool ReadBoundedFile(const std::wstring& path, std::string& output) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    char buf[513]{};
    DWORD got = 0;
    const BOOL ok = ReadFile(f, buf, sizeof(buf), &got, nullptr);
    CloseHandle(f);
    if (!ok || got == 0 || got > 512) return false;
    output.assign(buf, buf + got);
    return true;
}

void LogWindowState() {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar) {
        Wh_Log(L"Diagnostic: primary taskbar not found; no changes made");
        return;
    }
    RECT rect{};
    if (!GetWindowRect(taskbar, &rect)) return;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST), &mi)) {
        Wh_Log(L"Diagnostic: taskbar=(%ld,%ld)-(%ld,%ld); work=(%ld,%ld)-(%ld,%ld)",
               rect.left, rect.top, rect.right, rect.bottom,
               mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom);
    }
}

DWORD WINAPI DiagnosticThread(LPVOID) {
    const ULONGLONG started = GetTickCount64();
    while (WaitForSingleObject(g_stopEvent, 200) == WAIT_TIMEOUT) {
        if (GetTickCount64() - started >= 120000) {
            InterlockedExchange(&g_diagnosticInactive, 1);
            Wh_Log(L"Diagnostic: 120-second local time limit reached (no modifications to undo)");
            break;
        }
        if (!g_observer) continue;
        std::string leaseBytes;
        if (!ReadBoundedFile(g_leaseFile, leaseBytes) ||
            !g_observer->Observe(leaseBytes, NowUtcMilliseconds())) {
            InterlockedExchange(&g_diagnosticInactive, 1);
            Wh_Log(L"Diagnostic: missing/stale/invalid/mismatched watchdog lease; observing stopped");
            break;
        }
    }
    return 0;
}
} // namespace

BOOL Wh_ModInit() {
    InterlockedExchange(&g_diagnosticInactive, 0);
    LogWindowState();

    const PCWSTR configuredFile = Wh_GetStringSetting(L"diagnosticLeaseFile");
    const PCWSTR configuredId = Wh_GetStringSetting(L"diagnosticTrialId");
    std::wstring file = configuredFile ? configuredFile : L"";
    std::wstring id = configuredId ? configuredId : L"";
    if (configuredFile) Wh_FreeStringSetting(configuredFile);
    if (configuredId) Wh_FreeStringSetting(configuredId);

    if (!file.empty() || !id.empty()) {
        if (file.empty() || id.size() != 32) {
            Wh_Log(L"Diagnostic: lease file and 32-character trial ID must both be set");
            return FALSE;
        }
        std::string asciiId;
        for (wchar_t c : id) {
            if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) {
                Wh_Log(L"Diagnostic: invalid trial ID; nothing enabled");
                return FALSE;
            }
            asciiId.push_back(static_cast<char>(c));
        }
        g_leaseFile = std::move(file);
        g_observer.reset(new animated_taskbar::LeaseObserver(std::move(asciiId)));
    } else {
        Wh_Log(L"Diagnostic: no lease configured; observing window geometry only");
    }

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) return FALSE;
    g_timerThread = CreateThread(nullptr, 0, DiagnosticThread, nullptr, 0, nullptr);
    if (!g_timerThread) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        g_observer.reset();
        return FALSE;
    }
    Wh_Log(L"Diagnostic: 120-second read-only observation started");
    return TRUE;
}

void Wh_ModUninit() {
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_timerThread) {
        WaitForSingleObject(g_timerThread, INFINITE);
        CloseHandle(g_timerThread);
        g_timerThread = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
    g_observer.reset();
    g_leaseFile.clear();
    Wh_Log(L"Diagnostic: unloaded; no taskbar properties were modified");
}
