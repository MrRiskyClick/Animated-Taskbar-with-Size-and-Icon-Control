// ==WindhawkMod==
// @id animated-taskbar-size-icon-control
// @name Animated Taskbar (Safety-First Diagnostic Prototype)
// @description Non-mutating 120-second diagnostic prototype; does NOT modify the Windows taskbar
// @version 0.0.1
// @author Animated Taskbar project contributors
// @include explorer.exe
// @architecture x86-64
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Safety-first Stage 0 prototype

**This build is diagnostic-only.** It makes NO changes to taskbar height,
system tray, pinned app buttons, window placement, styles, or work area.

It observes the native taskbar window geometry when loaded and starts a
120-second local diagnostic timer. At expiry it becomes diagnostically
inactive; **it does NOT disable itself in the Windhawk UI**.

A local timer inside explorer.exe does NOT meet the project's mandated
independent watchdog, durable snapshot, automatic disable or rollback
requirements. Do not add active taskbar mutations before these are developed,
tested and reviewed. See docs/SAFETY.md.

No third-party code is copied into this file.
*/
// ==/WindhawkModReadme==

#include <windows.h>

namespace {
HANDLE g_stopEvent = nullptr;
HANDLE g_timerThread = nullptr;
volatile LONG g_trialExpired = 0;

void LogWindowState() {
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar) {
        Wh_Log(L"Stage-0: Shell_TrayWnd not present; no modification attempted");
        return;
    }

    RECT taskbarRect{};
    if (!GetWindowRect(taskbar, &taskbarRect)) {
        Wh_Log(L"Stage-0: GetWindowRect failed; no modification attempted");
        return;
    }

    HMONITOR monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(monitor, &mi)) {
        Wh_Log(L"Stage-0: taskbar=(%ld,%ld)-(%ld,%ld), work=(%ld,%ld)-(%ld,%ld)",
               taskbarRect.left, taskbarRect.top, taskbarRect.right,
               taskbarRect.bottom, mi.rcWork.left, mi.rcWork.top,
               mi.rcWork.right, mi.rcWork.bottom);
    }
}

DWORD WINAPI LocalTimerThread(LPVOID) {
    const DWORD outcome = WaitForSingleObject(g_stopEvent, 120000);
    if (outcome == WAIT_TIMEOUT) {
        InterlockedExchange(&g_trialExpired, 1);
        // IMPORTANT: This is not a Windhawk disable and cannot restore
        // anything if Explorer is not running. This prototype mutates nothing.
        Wh_Log(L"Stage-0: 120s diagnostic timer expired; no taskbar changes to revert");
    }
    return 0;
}
}  // namespace

BOOL Wh_ModInit() {
    InterlockedExchange(&g_trialExpired, 0);
    LogWindowState();

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        Wh_Log(L"Stage-0: timer event allocation failed");
        return FALSE;
    }

    g_timerThread = CreateThread(nullptr, 0, LocalTimerThread, nullptr, 0, nullptr);
    if (!g_timerThread) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        Wh_Log(L"Stage-0: timer thread allocation failed");
        return FALSE;
    }

    Wh_Log(L"Stage-0: diagnostic timer armed for 120 seconds; no modifications active");
    return TRUE;
}

void Wh_ModUninit() {
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }
    if (g_timerThread) {
        WaitForSingleObject(g_timerThread, INFINITE);
        CloseHandle(g_timerThread);
        g_timerThread = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }

    Wh_Log(L"Stage-0: diagnostic prototype unloaded; nothing to restore");
}
