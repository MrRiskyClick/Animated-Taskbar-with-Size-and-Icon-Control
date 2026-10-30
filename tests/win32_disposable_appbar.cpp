// Disposable GitHub-hosted Windows runner ONLY. Never run this on the user's PC.
// Proof-of-concept for Nectar's appbar reservation != actual HWND size.
// This program owns ONLY its throwaway HWND; never modifies Shell_TrayWnd.
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <cmath>
#include <cstdlib>

struct OwnAppBar {
    HWND hwnd = nullptr;
    bool registered = false;
    ~OwnAppBar() {
        if (registered && hwnd) {
            APPBARDATA data{sizeof(APPBARDATA)};
            data.hWnd = hwnd;
            SHAppBarMessage(ABM_REMOVE,&data);
        }
        if (hwnd) DestroyWindow(hwnd);
    }
};
static bool SameRect(const RECT& a, const RECT& b) {
    return a.left==b.left && a.top==b.top &&
           a.right==b.right && a.bottom==b.bottom;
}
int main() {
    // Headless GitHub-hosted Windows sessions may not have shell/appbar service.
    HWND shell = FindWindowW(L"Shell_TrayWnd",nullptr);
    if (!shell) {
        std::cout << "SKIP: no Windows desktop taskbar on CI runner\n";
        return 0;
    }
    MONITORINFO info{sizeof(info)};
    auto monitor = MonitorFromWindow(shell,MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(monitor,&info)) {
        std::cout << "SKIP: monitor workarea unavailable in CI\n";
        return 0;
    }
    const RECT initialWork=info.rcWork;
    const RECT display=info.rcMonitor;
    const int reservePx=42, visualPx=96;
    {
    OwnAppBar bar;
    bar.hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",
        L"ATB GitHub CI temporary appbar proof",WS_POPUP,
        display.left,display.bottom-visualPx,
        display.right-display.left,visualPx,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!bar.hwnd){std::cerr<<"CreateWindowEx failed\n";return 1;}

    APPBARDATA abd{sizeof(APPBARDATA)};
    abd.hWnd=bar.hwnd;
    abd.uCallbackMessage=WM_APP+44;
    if(!SHAppBarMessage(ABM_NEW,&abd)) {
        std::cout<<"SKIP: CI session lacks appbar reservation service\n";
        return 0;
    }
    bar.registered=true;
    abd.uEdge=ABE_BOTTOM;
    abd.rc={display.left,display.bottom-reservePx,display.right,display.bottom};
    SHAppBarMessage(ABM_QUERYPOS,&abd);
    abd.rc.top=abd.rc.bottom-reservePx;
    if(!SHAppBarMessage(ABM_SETPOS,&abd)){
        std::cerr<<"ABM_SETPOS failed\n";return 1;
    }
    if(!SetWindowPos(bar.hwnd,HWND_TOPMOST,abd.rc.left,
                     display.bottom-visualPx,
                     display.right-display.left,visualPx,
                     SWP_NOACTIVATE|SWP_NOOWNERZORDER)) {
        std::cerr<<"SetWindowPos failed\n";return 1;
    }
    RECT actual{};
    if(!GetWindowRect(bar.hwnd,&actual)){return 1;}
    if(actual.bottom-actual.top!=visualPx){return 1;}
    MONITORINFO current{sizeof(current)};
    if(!GetMonitorInfoW(monitor,&current)){return 1;}
    const int workDelta=initialWork.bottom-current.rcWork.bottom;
    std::cout<<"OWNED HWND physical height: "<<visualPx
             <<"px, reserved rectangle: "<<reservePx
             <<"px; workarea delta: "<<workDelta<<"px\n";
    // Different shells may coalesce appbars, so don't claim a precise
    // reservation if the shell didn't report the requested area. This is
    // a diagnostic, not proof about the real Shell_TrayWnd.
    if(workDelta != reservePx) {
        std::cout<<"INCONCLUSIVE: CI work-area did not reserve precisely 42px\n";
        return 0;
    }
    if(SameRect(initialWork,current.rcWork)){return 1;}
    std::cout<<"PASS: independent visual HWND 96px and desktop reservation 42px\n";
    } // Owned HWND destroyed; ABM_REMOVE called before verifying rollback.

    // Automatic restoration must be observable after disposal of the owned
    // appbar, not merely assumed from successful registration. This is still
    // a disposable application-owned HWND, NEVER Shell_TrayWnd.
    MONITORINFO restored{sizeof(restored)};
    bool recovered=false;
    for (int retries=0; retries<75; ++retries) {
        restored.cbSize=sizeof(restored);
        if (!GetMonitorInfoW(monitor,&restored)) {
            std::cerr<<"GetMonitorInfoW failed during restoration verification\n";
            return 1;
        }
        if (SameRect(initialWork,restored.rcWork)) {
            recovered=true;
            break;
        }
        Sleep(40);
    }
    if (!recovered) {
        std::cerr<<"FAIL: OWNED appbar work area did not restore after ABM_REMOVE\n";
        return 1;
    }
    std::cout<<"PASS: owned appbar removed; desktop work area restored exactly\n";
    return 0;
}
