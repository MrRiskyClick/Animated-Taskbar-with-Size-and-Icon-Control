// ==WindhawkMod==
// @id atb-native-taskbar-inventory
// @name Animated Taskbar - Native Button Inventory (Read Only)
// @description Inspects native Windows 11 taskbar XAML button geometry and clipping on pointer move, with no visual changes.
// @version 0.1.0
// @author Animated Taskbar project contributors
// @include explorer.exe
// @architecture x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lshcore -lwindowsapp -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Animated Taskbar: native inventory (Stage 2)
Move the mouse over the native Windows 11 taskbar to trigger a bounded, one-time
XAML inventory. Windhawk debug output logs relevant class names, dimensions,
clip geometry, button counts and the taskbar/work area dimensions.

This build deliberately performs NO XAML property writes, native button movement,
taskbar height changes, registry edits, or application launching. It does NOT
enable the visual dock. This is not an active trial and doesn't need rollback.

The 120-second limit is only on *observation*, not real restoration.
The actual animation trial remains gated on independently tested recovery.
Never run this concurrently with a second local copy of the same mod.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>
#undef GetCurrentTime
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <atomic>
#include <string>
#include <string_view>
#include <algorithm>
#include <cwchar>

namespace Xaml = winrt::Windows::UI::Xaml;
namespace Media = winrt::Windows::UI::Xaml::Media;

namespace {
std::atomic<bool> g_symbolsRegistered{false};
std::atomic<bool> g_moduleFound{false};
std::atomic<bool> g_snapshotStarted{false};
ULONGLONG g_started = 0;
using TaskbarFramePointerMoved = int(WINAPI*)(void*, void*);
TaskbarFramePointerMoved g_pointerMovedOriginal = nullptr;

bool IsRelevant(const std::wstring& s) {
    return s.find(L"Taskbar.") != std::wstring::npos ||
           s.find(L"ScrollContentPresenter") != std::wstring::npos ||
           s.find(L"ItemsRepeater") != std::wstring::npos ||
           s.find(L"ScrollViewer") != std::wstring::npos ||
           s.find(L"LayoutRoot") != std::wstring::npos ||
           s.find(L"RootGrid") != std::wstring::npos;
}

struct SnapshotCounters {
    unsigned totalNodes = 0;
    unsigned taskButtons = 0;
    unsigned otherTaskbarNodes = 0;
    unsigned clipGeometries = 0;
    unsigned logged = 0;
};

void Visit(Xaml::FrameworkElement const& element, unsigned depth,
           SnapshotCounters& count) {
    if (!element || depth > 12 || count.totalNodes >= 600) return;
    ++count.totalNodes;
    auto className = winrt::get_class_name(element);
    std::wstring name = className.c_str();
    if (name == L"Taskbar.TaskListButton") ++count.taskButtons;
    if (name.find(L"Taskbar.") != std::wstring::npos) ++count.otherTaskbarNodes;

    const bool relevant = IsRelevant(name);
    if (relevant && count.logged < 95) {
        bool clipped = !!element.Clip();
        if (clipped) ++count.clipGeometries;
        const auto rectClip = element.Clip().try_as<Media::RectangleGeometry>();
        if (rectClip) {
            auto r = rectClip.Rect();
            Wh_Log(L"[ATB INVENTORY] depth=%u class=%s size=%.1fx%.1f clipRect=(%.1f,%.1f,%.1f,%.1f)",
                   depth, name.c_str(), element.ActualWidth(), element.ActualHeight(),
                   r.X, r.Y, r.Width, r.Height);
        } else {
            Wh_Log(L"[ATB INVENTORY] depth=%u class=%s size=%.1fx%.1f clip=%d",
                   depth, name.c_str(), element.ActualWidth(), element.ActualHeight(),
                   clipped ? 1 : 0);
        }
        ++count.logged;
    }

    const int children = Media::VisualTreeHelper::GetChildrenCount(element);
    for (int i = 0; i < children && count.totalNodes < 600; ++i) {
        auto child = Media::VisualTreeHelper::GetChild(element, i)
                         .try_as<Xaml::FrameworkElement>();
        if (child) Visit(child, depth+1, count);
    }
}

void LogWorkArea() {
    HWND hwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hwnd) { Wh_Log(L"[ATB INVENTORY] primary HWND unavailable"); return; }
    RECT r{};
    if (!GetWindowRect(hwnd, &r)) return;
    MONITORINFO m{sizeof(MONITORINFO)};
    if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &m)) {
        Wh_Log(L"[ATB INVENTORY] HWND=(%ld,%ld)-(%ld,%ld) work=(%ld,%ld)-(%ld,%ld)",
               r.left,r.top,r.right,r.bottom,
               m.rcWork.left,m.rcWork.top,m.rcWork.right,m.rcWork.bottom);
    }
}

int WINAPI PointerMovedHook(void* self, void* args) {
    const auto next = [&]() {
        return g_pointerMovedOriginal(self, args);
    };
    if (GetTickCount64() - g_started >= 120000 || g_snapshotStarted.load()) {
        return next();
    }

    try {
        Xaml::FrameworkElement frame = nullptr;
        ((IUnknown*)self)->QueryInterface(winrt::guid_of<Xaml::FrameworkElement>(),
                                        winrt::put_abi(frame));
        if (!frame || winrt::get_class_name(frame) != L"Taskbar.TaskbarFrame")
            return next();

        if (!g_snapshotStarted.exchange(true)) {
            Wh_Log(L"[ATB INVENTORY] native frame found; beginning read-only survey");
            LogWorkArea();
            SnapshotCounters count;
            Visit(frame, 0, count);
            Wh_Log(L"[ATB INVENTORY] nodes=%u nativeTaskButtons=%u taskbarNodes=%u clipGeometries=%u logEntries=%u",
                   count.totalNodes, count.taskButtons, count.otherTaskbarNodes,
                   count.clipGeometries, count.logged);
            Wh_Log(L"[ATB INVENTORY] completed; the mod made NO visual changes");
        }
    } catch (winrt::hresult_error const& e) {
        Wh_Log(L"[ATB INVENTORY] XAML read failed: HRESULT=0x%08X", (unsigned)e.code());
    } catch (...) {
        Wh_Log(L"[ATB INVENTORY] XAML read failed with unknown exception");
    }
    return next();
}

bool HookNativeModule(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {{
        {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarFrame,struct winrt::Windows::UI::Xaml::Controls::IControlOverrides>::OnPointerMoved(void *))"},
        &g_pointerMovedOriginal,
        PointerMovedHook
    }};
    if (!HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"[ATB INVENTORY] TaskbarFrame symbol unavailable; no changes");
        return false;
    }
    Wh_Log(L"[ATB INVENTORY] native pointer hook registered");
    return true;
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t g_loadLibraryExWOriginal = nullptr;
HMODULE NativeModule() {
    HMODULE m = GetModuleHandleW(L"Taskbar.View.dll");
    return m ? m : GetModuleHandleW(L"ExplorerExtensions.dll");
}
HMODULE WINAPI LoadLibraryExWHook(LPCWSTR path, HANDLE file, DWORD flags) {
    HMODULE result = g_loadLibraryExWOriginal(path, file, flags);
    if (result && NativeModule() == result &&
        !g_moduleFound.exchange(true)) {
        if (HookNativeModule(result)) {
            Wh_ApplyHookOperations();
            g_symbolsRegistered = true;
        }
    }
    return result;
}
}

BOOL Wh_ModInit() {
    g_started = GetTickCount64();
    LogWorkArea();
    Wh_Log(L"[ATB INVENTORY] read-only native inspection armed for 120 seconds");
    if (HMODULE m = NativeModule()) {
        g_moduleFound = true;
        return HookNativeModule(m);
    }
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    auto p = (decltype(&LoadLibraryExW))GetProcAddress(kb, "LoadLibraryExW");
    if (!p) return FALSE;
    return WindhawkUtils::SetFunctionHook(p, LoadLibraryExWHook,
                                          &g_loadLibraryExWOriginal);
}

void Wh_ModAfterInit() {
    Wh_Log(L"[ATB INVENTORY] initialized; hover taskbar once to record native tree");
}

void Wh_ModUninit() {
    Wh_Log(L"[ATB INVENTORY] unloaded, no geometry or appearance to restore");
}
