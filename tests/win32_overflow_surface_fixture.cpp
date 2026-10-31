// CI ONLY: no calls to FindWindow("Shell_TrayWnd"), taskbar XAML,
// Windows appbar APIs, global hooks, process termination or registry.
// Tests ONLY this process's explicitly created disposable HWNDs.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include "../experiments/native-overflow-surface.hpp"

#define REQUIRE(test, message) do { if (!(test)) { \
    std::fprintf(stderr,"FAIL: %s (line %d, Win32 %lu)\n", \
        message,__LINE__,GetLastError()); return 1; \
} } while(0)

static LRESULT CALLBACK DummyProc(HWND hwnd,UINT msg,
                                  WPARAM w,LPARAM l){
    return DefWindowProcW(hwnd,msg,w,l);
}

int main(){
    WNDCLASSEXW cls{};
    cls.cbSize=sizeof(cls);
    cls.lpfnWndProc=DummyProc;
    cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"ATB.DisposableDummyTaskbar.1";
    REQUIRE(RegisterClassExW(&cls)!=0,"Register own dummy class");
    HWND dummy=CreateWindowExW(0,cls.lpszClassName,L"CI dummy, not Explorer",
        WS_OVERLAPPEDWINDOW,32,32,540,70,nullptr,nullptr,
        cls.hInstance,nullptr);
    REQUIRE(dummy!=nullptr,"Create own dummy HWND");
    ShowWindow(dummy,SW_SHOWNOACTIVATE);
    RECT original{};
    REQUIRE(GetWindowRect(dummy,&original),"Read own dummy rect");
    auto dummyStyle=GetWindowLongPtrW(dummy,GWL_EXSTYLE);

    atb_overlay::Surface overlay;
    RECT overflow{40,8,340,32}; // 24 px visual extension ABOVE base
    REQUIRE(overlay.Create(overflow),"Create own layered overlay");
    HWND owned=overlay.Handle();
    REQUIRE(owned && IsWindow(owned),"New overlay HWND exists");
    REQUIRE(!overlay.Visible(),"Default is hidden until safe");
    DWORD ex=(DWORD)GetWindowLongPtrW(owned,GWL_EXSTYLE);
    REQUIRE((ex&WS_EX_LAYERED)!=0,"Layered per-pixel compositor required");
    REQUIRE((ex&WS_EX_TRANSPARENT)!=0,"Full-window click-through required");
    REQUIRE((ex&WS_EX_NOACTIVATE)!=0,"No app/game focus theft");
    REQUIRE((ex&WS_EX_TOOLWINDOW)!=0,"No second taskbar/Alt-Tab entry");
    REQUIRE((ex&WS_EX_APPWINDOW)==0,"No app window style");
    REQUIRE(SendMessageW(owned,WM_NCHITTEST,0,0)==HTTRANSPARENT,
            "Backup nonclient click-through");
    REQUIRE(SendMessageW(owned,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE,
            "Backup activation guard");

    HICON app=LoadIconW(nullptr,MAKEINTRESOURCEW(32512));
    REQUIRE(app!=nullptr,"Windows stock icon available");
    atb_overlay::IconSprite icons[]={{app,22,-4,32,32},{app,86,0,22,22}};
    REQUIRE(overlay.Render(icons,2),"Layered icon surface render");
    REQUIRE(overlay.ShowIfSafe(false,false),"Unknown/hidden taskbar gate");
    REQUIRE(!overlay.Visible(),"Overlay hidden when taskbar hidden");
    REQUIRE(overlay.ShowIfSafe(true,true),"Fullscreen gate");
    REQUIRE(!overlay.Visible(),"Overlay hidden over fullscreen");
    REQUIRE(overlay.ShowIfSafe(true,false),"Safe explicit visible state");
    REQUIRE(overlay.Visible(),"Overlay visible outside fullscreen");
    REQUIRE(overlay.Render(nullptr,0),"Alpha-clear overlay without icons");

    RECT changed{36,7,360,42};
    REQUIRE(overlay.MoveTo(changed),"Resize own overlay only");
    REQUIRE(overlay.Render(icons,2),"Render after own DIB resize");
    REQUIRE(overlay.Bounds().left==36 && overlay.Bounds().bottom==42,
            "Overlay geometry updated");
    RECT after{};
    REQUIRE(GetWindowRect(dummy,&after),"Read own dummy rect again");
    REQUIRE(EqualRect(&original,&after)!=0,"Underlying taskbar rect unchanged");
    REQUIRE(GetWindowLongPtrW(dummy,GWL_EXSTYLE)==dummyStyle,
            "Underlying taskbar styles unchanged");
    overlay.Destroy();
    REQUIRE(!IsWindow(owned),"Overlay HWND destroyed");
    REQUIRE(IsWindow(dummy),"Underlying window survives overlay shutdown");
    DestroyWindow(dummy);
    std::puts("PASS: owned 32-bit layered overflow HWND, noactivation,");
    std::puts("      unconditional pointer transparency, fullscreen gating,");
    std::puts("      icon render, resize and native dummy HWND isolation");
    return 0;
}
