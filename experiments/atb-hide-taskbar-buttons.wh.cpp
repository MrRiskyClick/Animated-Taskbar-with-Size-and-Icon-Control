// ==WindhawkMod==
// @id              atb-hide-taskbar-buttons
// @name            ATB Hide Taskbar Buttons
// @description     Hide Start, Task View, pinned and running taskbar buttons without changing the system tray.
// @version         0.1.0
// @author          ATB contributors (based on Hide Start Button by ptrkhh)
// @github          https://github.com/MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control
// @license         GPL-3.0-or-later
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject
// ==/WindhawkMod==

// Source adapted from ptrkhh's Hide Start Button Windhawk mod
// https://github.com/ramensoftware/windhawk-mods/blob/main/mods/hide-start-button.wh.cpp
// Based in part on m417z's Start button always on the left.
// Preserves XAML object ownership and UI-thread dispatch techniques.
// Distributed under GPL-3.0-or-later.
//
// ==WindhawkModReadme==
/*
# ATB Hide Taskbar Buttons
Developer-only source for an independent Enable/Disable toggle.

Hides: Windows Start, Task View, running applications, and pinned applications.
Does not target the system tray, clock, volume, network or notification area.
Does not resize Shell_TrayWnd, change its region, alter maximized work area,
use XAML diagnostics, or modify Windhawk Taskbar Styler settings.

This mod works through Taskbar.View native XAML layout hooks, not a second
XAML diagnostics consumer. It is therefore intended to coexist with
Windows 11 Taskbar Styler (e.g. SimplyTransparent); exact coexistence and
unload/restore behavior still require USER testing.

The hidden Windows controls remain accessible by Windows key / Win+Tab.
This mod does not change pinned app data, process states or registry.

Only Windows 11 is supported. This source has NOT yet been user-verified.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

#include <atomic>
#include <vector>
#include <cmath>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Xaml.h>

using namespace winrt::Windows::UI::Xaml;

std::atomic<bool> g_taskbarViewDllLoaded;
std::atomic<bool> g_unloading;
thread_local bool g_inArrangeOverride;

template <typename F>
FrameworkElement EnumChildElements(FrameworkElement element, F callback) {
    int count = Media::VisualTreeHelper::GetChildrenCount(element);
    for (int i = 0; i < count; i++) {
        auto child = Media::VisualTreeHelper::GetChild(element, i)
                         .try_as<FrameworkElement>();
        if (child && callback(child))
            return child;
    }
    return nullptr;
}

FrameworkElement FindChildByName(FrameworkElement element, PCWSTR name) {
    return EnumChildElements(element, [name](FrameworkElement child) {
        return child.Name() == name;
    });
}

// Each monitor may use a separate XAML UI thread. Store only WEAK references
// and original properties per owning thread so we never pin taskbar controls
// or cross WinRT apartments during unload.
struct OriginalButton {
    winrt::weak_ref<FrameworkElement> weak;
    Visibility visibility;
    double width;
    double height;
};
thread_local std::vector<OriginalButton> g_originalButtons;

bool IsHideTarget(FrameworkElement const& element) {
    auto type=winrt::get_class_name(element);
    if(type==L"Taskbar.TaskListButton" ||
       type==L"Taskbar.TaskListLabeledButton") return true;

    if(type!=L"Taskbar.ExperienceToggleButton")return false;
    auto id=Automation::AutomationProperties::GetAutomationId(element);
    return id==L"StartButton" || id==L"TaskViewButton";
}

void HideTarget(FrameworkElement const& element) {
    if(g_unloading || !IsHideTarget(element))return;
    for(auto const& entry:g_originalButtons) {
        auto existing=entry.weak.get();
        if(existing && winrt::get_abi(existing)==winrt::get_abi(element))
            return;
    }
    const bool appButton=winrt::get_class_name(element)!=
                           L"Taskbar.ExperienceToggleButton";
    g_originalButtons.push_back({
        winrt::make_weak(element),element.Visibility(),
        element.Width(),element.Height()
    });
    // Preserve the exact original state, and do not change the shell
    // window, its region, or Windows' desktop reservation.
    element.Visibility(Visibility::Collapsed);
    if(appButton) {
        element.Width(0.0);
        element.Height(0.0);
    }
}
void RestoreButtonsOnThisThread() {
    // Only the owning XAML UI thread may call into these elements.
    for(auto const& original:g_originalButtons) {
        auto element=original.weak.get();
        if(!element)continue;
        element.Width(original.width);
        element.Height(original.height);
        element.Visibility(original.visibility);
    }
    g_originalButtons.clear();
}

void VisitTaskbarTree(FrameworkElement const& element,int depth) {
    if(!element || depth>24)return;
    HideTarget(element);
    int count=Media::VisualTreeHelper::GetChildrenCount(element);
    for(int i=0;i<count;i++) {
        auto child=Media::VisualTreeHelper::GetChild(element,i)
                     .try_as<FrameworkElement>();
        if(child) VisitTaskbarTree(child,depth+1);
    }
}
bool ApplyStyle(XamlRoot xamlRoot) {
    if(g_unloading) {
        RestoreButtonsOnThisThread();
        return true;
    }
    auto root=xamlRoot.Content().try_as<FrameworkElement>();
    if(!root)return false;
    // RootGrid contains the native taskbar buttons. Do NOT manipulate
    // Shell_TrayWnd, appbar geometry, tray or clock.
    auto frame=EnumChildElements(root,[](FrameworkElement c){
        return winrt::get_class_name(c)==L"Taskbar.TaskbarFrame";
    });
    if(!frame)return false;
    auto content=FindChildByName(frame,L"RootGrid");
    if(!content)return false;
    VisitTaskbarTree(content,0);
    return true;
}

void* CTaskBand_ITaskListWndSite_vftable;
void* CSecondaryTaskBand_ITaskListWndSite_vftable;

using CTaskBand_GetTaskbarHost_t = void*(WINAPI*)(void* pThis, void** result);
CTaskBand_GetTaskbarHost_t CTaskBand_GetTaskbarHost_Original;

void* TaskbarHost_FrameHeight_Original;

using CSecondaryTaskBand_GetTaskbarHost_t = void*(WINAPI*)(void* pThis,
                                                           void** result);
CSecondaryTaskBand_GetTaskbarHost_t CSecondaryTaskBand_GetTaskbarHost_Original;

using std__Ref_count_base__Decref_t = void(WINAPI*)(void* pThis);
std__Ref_count_base__Decref_t std__Ref_count_base__Decref_Original;

XamlRoot XamlRootFromTaskbarHostSharedPtr(void* taskbarHostSharedPtr[2]) {
    if (!taskbarHostSharedPtr[0] && !taskbarHostSharedPtr[1])
        return nullptr;

    size_t offset = 0x48;
    const BYTE* b = (const BYTE*)TaskbarHost_FrameHeight_Original;
    if (b[0] == 0x48 && b[1] == 0x83 && b[2] == 0xEC && b[4] == 0x48 &&
        b[5] == 0x83 && b[6] == 0xC1 && b[7] <= 0x7F)
        offset = b[7];

    auto* unk = *(IUnknown**)((BYTE*)taskbarHostSharedPtr[0] + offset);
    FrameworkElement fe = nullptr;
    unk->QueryInterface(winrt::guid_of<FrameworkElement>(),
                        winrt::put_abi(fe));

    auto result = fe ? fe.XamlRoot() : nullptr;
    std__Ref_count_base__Decref_Original(taskbarHostSharedPtr[1]);
    return result;
}

XamlRoot GetTaskbarXamlRoot(HWND hWnd, bool isSecondary) {
    HWND hTaskSwWnd = isSecondary
        ? (HWND)FindWindowEx(hWnd, nullptr, L"WorkerW", nullptr)
        : (HWND)GetProp(hWnd, L"TaskbandHWND");
    if (!hTaskSwWnd)
        return nullptr;

    void* vftable = isSecondary ? CSecondaryTaskBand_ITaskListWndSite_vftable
                                : CTaskBand_ITaskListWndSite_vftable;
    void* p = (void*)GetWindowLongPtr(hTaskSwWnd, 0);
    for (int i = 0; *(void**)p != vftable; i++) {
        if (i == 20)
            return nullptr;
        p = (void**)p + 1;
    }

    void* sharedPtr[2]{};
    if (isSecondary)
        CSecondaryTaskBand_GetTaskbarHost_Original(p, sharedPtr);
    else
        CTaskBand_GetTaskbarHost_Original(p, sharedPtr);
    return XamlRootFromTaskbarHostSharedPtr(sharedPtr);
}

HWND FindCurrentProcessTaskbarWnd() {
    HWND hWnd = nullptr;
    while ((hWnd = FindWindowEx(nullptr, hWnd, L"Shell_TrayWnd", nullptr))) {
        DWORD pid;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid == GetCurrentProcessId())
            return hWnd;
    }
    return nullptr;
}

void ApplySettingsFromTaskbarThread() {
    EnumThreadWindows(
        GetCurrentThreadId(),
        [](HWND hWnd, LPARAM) -> BOOL {
            WCHAR cls[32];
            if (GetClassName(hWnd, cls, ARRAYSIZE(cls)) == 0)
                return TRUE;

            XamlRoot xamlRoot = nullptr;
            if (_wcsicmp(cls, L"Shell_TrayWnd") == 0)
                xamlRoot = GetTaskbarXamlRoot(hWnd, false);
            else if (_wcsicmp(cls, L"Shell_SecondaryTrayWnd") == 0)
                xamlRoot = GetTaskbarXamlRoot(hWnd, true);

            if (xamlRoot)
                ApplyStyle(xamlRoot);
            return TRUE;
        },
        0);
}

void ApplySettings(HWND hTaskbarWnd) {
    static const UINT msg =
        RegisterWindowMessage(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);

    DWORD threadId = GetWindowThreadProcessId(hTaskbarWnd, nullptr);
    if (!threadId)
        return;

    if (threadId == GetCurrentThreadId()) {
        ApplySettingsFromTaskbarThread();
        return;
    }

    HHOOK hook = SetWindowsHookEx(
        WH_CALLWNDPROC,
        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (nCode == HC_ACTION &&
                ((const CWPSTRUCT*)lParam)->message == msg)
                ApplySettingsFromTaskbarThread();
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        },
        nullptr, threadId);
    if (!hook)
        return;

    SendMessage(hTaskbarWnd, msg, 0, 0);
    UnhookWindowsHookEx(hook);
}

using IUIElement_Arrange_t =
    HRESULT(WINAPI*)(void* pThis, winrt::Windows::Foundation::Rect rect);
IUIElement_Arrange_t IUIElement_Arrange_Original;
HRESULT WINAPI IUIElement_Arrange_Hook(void* pThis,
                                       winrt::Windows::Foundation::Rect rect) {
    auto original = [=] { return IUIElement_Arrange_Original(pThis, rect); };

    if (!g_inArrangeOverride || g_unloading)
        return original();

    FrameworkElement element = nullptr;
    ((IUnknown*)pThis)
        ->QueryInterface(winrt::guid_of<FrameworkElement>(),
                         winrt::put_abi(element));
    if (!element)
        return original();

    if(!IsHideTarget(element))return original();
    HideTarget(element);
    // Native collapsible layout can allocate space even when the target
    // is hidden. Its original arrange slot gets zero width as well.
    return IUIElement_Arrange_Original(pThis,
                                      {rect.X,rect.Y,0,rect.Height});
}

using TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_t =
    HRESULT(WINAPI*)(void* pThis,
                     void* context,
                     winrt::Windows::Foundation::Size size,
                     winrt::Windows::Foundation::Size* resultSize);
TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_t
    TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original;
HRESULT WINAPI TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Hook(
    void* pThis,
    void* context,
    winrt::Windows::Foundation::Size size,
    winrt::Windows::Foundation::Size* resultSize) {
    [[maybe_unused]] static bool hooked = [] {
        Shapes::Rectangle rectangle;
        IUIElement element = rectangle;
        void** vtable = *(void***)winrt::get_abi(element);
        WindhawkUtils::SetFunctionHook((IUIElement_Arrange_t)vtable[92],
                                       IUIElement_Arrange_Hook,
                                       &IUIElement_Arrange_Original);
        Wh_ApplyHookOperations();
        return true;
    }();

    if(g_unloading)return TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original(
        pThis,context,size,resultSize);
    g_inArrangeOverride = true;
    HRESULT ret = TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original(
        pThis, context, size, resultSize);
    g_inArrangeOverride = false;
    return ret;
}

bool HookTaskbarDllSymbols() {
    HMODULE module =
        LoadLibraryEx(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
        return false;

    WindhawkUtils::SYMBOL_HOOK taskbarDllHooks[] = {
        {{LR"(const CTaskBand::`vftable'{for `ITaskListWndSite'})"},
         &CTaskBand_ITaskListWndSite_vftable},
        {{LR"(const CSecondaryTaskBand::`vftable'{for `ITaskListWndSite'})"},
         &CSecondaryTaskBand_ITaskListWndSite_vftable},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CTaskBand::GetTaskbarHost(void)const )"},
         &CTaskBand_GetTaskbarHost_Original},
        {{LR"(public: int __cdecl TaskbarHost::FrameHeight(void)const )"},
         &TaskbarHost_FrameHeight_Original},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CSecondaryTaskBand::GetTaskbarHost(void)const )"},
         &CSecondaryTaskBand_GetTaskbarHost_Original},
        {{LR"(public: void __cdecl std::_Ref_count_base::_Decref(void))"},
         &std__Ref_count_base__Decref_Original},
    };
    return HookSymbols(module, taskbarDllHooks, ARRAYSIZE(taskbarDllHooks));
}

bool HookTaskbarViewDllSymbols(HMODULE module) {
    // Taskbar.View.dll, ExplorerExtensions.dll
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {{LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarCollapsibleLayout,struct winrt::Microsoft::UI::Xaml::Controls::IVirtualizingLayoutOverrides>::ArrangeOverride(void *,struct winrt::Windows::Foundation::Size,struct winrt::Windows::Foundation::Size *))"},
         &TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Original,
         TaskbarCollapsibleLayoutXamlTraits_ArrangeOverride_Hook},
    };
    return HookSymbols(module, hooks, ARRAYSIZE(hooks));
}

HMODULE GetTaskbarViewModuleHandle() {
    HMODULE m = GetModuleHandle(L"Taskbar.View.dll");
    return m ? m : GetModuleHandle(L"ExplorerExtensions.dll");
}

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;
HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR lpLibFileName,
                                   HANDLE hFile,
                                   DWORD dwFlags) {
    HMODULE module = LoadLibraryExW_Original(lpLibFileName, hFile, dwFlags);
    if (module && !g_taskbarViewDllLoaded &&
        GetTaskbarViewModuleHandle() == module &&
        !g_taskbarViewDllLoaded.exchange(true)) {
        if (HookTaskbarViewDllSymbols(module))
            Wh_ApplyHookOperations();
    }
    return module;
}

BOOL Wh_ModInit() {
    if (!HookTaskbarDllSymbols())
        return FALSE;

    if (HMODULE m = GetTaskbarViewModuleHandle()) {
        g_taskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(m))
            return FALSE;
    } else {
        HMODULE kb = GetModuleHandle(L"kernelbase.dll");
        auto pLoadLib =
            (decltype(&LoadLibraryExW))GetProcAddress(kb, "LoadLibraryExW");
        WindhawkUtils::SetFunctionHook(pLoadLib, LoadLibraryExW_Hook,
                                       &LoadLibraryExW_Original);
    }
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_taskbarViewDllLoaded) {
        if (HMODULE m = GetTaskbarViewModuleHandle()) {
            if (!g_taskbarViewDllLoaded.exchange(true)) {
                if (HookTaskbarViewDllSymbols(m))
                    Wh_ApplyHookOperations();
            }
        }
    }

    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (hTaskbarWnd)
        ApplySettings(hTaskbarWnd);
}

void Wh_ModBeforeUninit() {
    g_unloading = true;
    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (hTaskbarWnd)
        ApplySettings(hTaskbarWnd);
}

void Wh_ModUninit() {}
