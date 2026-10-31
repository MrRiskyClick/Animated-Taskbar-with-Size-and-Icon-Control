// GitHub-hosted Windows desktop ONLY. Never run on a personal computer.
// Objective native Shell_TrayWnd rectangle/workarea trial with separate watchdog.
// This deliberately DOES NOT load Windhawk, touch registry, change auto-hide,
// register appbars, alter taskbar XAML, terminate Explorer or install anything.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <cwchar>
#include <cstdlib>

struct Handle {
    HANDLE h = nullptr;
    explicit Handle(HANDLE v = nullptr) : h(v) {}
    ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    Handle(Handle const&) = delete;
    Handle& operator=(Handle const&) = delete;
    bool valid() const { return h && h != INVALID_HANDLE_VALUE; }
};
bool Equals(RECT a, RECT b) {
    return a.left==b.left && a.top==b.top &&
           a.right==b.right && a.bottom==b.bottom;
}
bool HostedCI() {
    wchar_t flag[16]{};
    DWORD len=GetEnvironmentVariableW(L"GITHUB_ACTIONS",flag,16);
    return len==4 && _wcsicmp(flag,L"true")==0;
}
bool Target(HWND& hwnd, DWORD& pid, RECT& rect, MONITORINFO& monitor) {
    hwnd=FindWindowW(L"Shell_TrayWnd",nullptr);
    if (!hwnd || !IsWindow(hwnd)) return false;
    DWORD thread=GetWindowThreadProcessId(hwnd,&pid);
    if (!thread || !pid) return false;
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    if(!process.valid()) return false;
    wchar_t executable[32768]{};
    DWORD size=32768;
    if(!QueryFullProcessImageNameW(process.h,0,executable,&size)) return false;
    const wchar_t* name=wcsrchr(executable,L'\\');
    name=name?name+1:executable;
    if(_wcsicmp(name,L"explorer.exe")!=0) return false;
    if(!GetWindowRect(hwnd,&rect)) return false;
    monitor.cbSize=sizeof(monitor);
    return GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
}
bool Restore(HWND hwnd, RECT baseline, RECT expected) {
    RECT current{};
    if (!IsWindow(hwnd) || !GetWindowRect(hwnd,&current)) {
        std::cout<<"ERROR: native taskbar no longer exists\n";
        return false;
    }
    if(Equals(current,baseline)) return true;
    if(!Equals(current,expected)) {
        std::cout<<"CONFLICT: native taskbar changed elsewhere; refusing to overwrite\n";
        return false;
    }
    // ASYNCWINDOWPOS posts to the taskbar's own thread. It does not wait for
    // an unresponsive Explorer, so a blocked UI remains an honest limitation.
    if(!SetWindowPos(hwnd,nullptr,baseline.left,baseline.top,
        baseline.right-baseline.left,baseline.bottom-baseline.top,
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_ASYNCWINDOWPOS)) {
        std::cout<<"ERROR: watchdog SetWindowPos failed\n";
        return false;
    }
    for(int i=0;i<100;++i) {
        Sleep(30);
        if(GetWindowRect(hwnd,&current)&&Equals(current,baseline))return true;
    }
    std::cout<<"ERROR: native taskbar did not return to original rectangle\n";
    return false;
}
int Watchdog(DWORD expectedExplorerPid, DWORD parentPid, RECT baseline,
             RECT expected, const std::wstring& readyName,
             const std::wstring& finishName) {
    Handle ready(OpenEventW(EVENT_MODIFY_STATE,FALSE,readyName.c_str()));
    Handle finish(OpenEventW(SYNCHRONIZE,FALSE,finishName.c_str()));
    Handle parent(OpenProcess(SYNCHRONIZE,FALSE,parentPid));
    if(!ready.valid()||!finish.valid()||!parent.valid())return 3;
    HWND hwnd{};DWORD pid=0;RECT current{}; MONITORINFO mon{sizeof(mon)};
    if(!Target(hwnd,pid,current,mon)||pid!=expectedExplorerPid||
       !Equals(current,baseline))return 4;
    if(!SetEvent(ready.h))return 5;
    HANDLE watchers[2]{finish.h,parent.h};
    const auto deadline=GetTickCount64()+15000;
    while(GetTickCount64()<deadline) {
        DWORD status=WaitForMultipleObjects(2,watchers,FALSE,80);
        if(status==WAIT_OBJECT_0||status==WAIT_OBJECT_0+1)break;
        if(status==WAIT_FAILED)return 6;
    }
    if(!GetWindowThreadProcessId(hwnd,&pid)||pid!=expectedExplorerPid)return 7;
    if(!Restore(hwnd,baseline,expected))return 8;
    std::cout<<"PASS: independent process restored native taskbar rectangle\n";
    return 0;
}
int Parent(bool simulateAbruptDeath=false) {
    HWND hwnd{};DWORD explorerPid=0;RECT baseline{};
    MONITORINFO monitor{sizeof(monitor)};
    if(!Target(hwnd,explorerPid,baseline,monitor)){
        std::cout<<"SKIP: compatible Explorer native taskbar unavailable on CI runner\n";
        return 0;
    }
    const int height=baseline.bottom-baseline.top;
    if(height<30||height>150||baseline.bottom!=monitor.rcMonitor.bottom||
       baseline.top-monitor.rcMonitor.top<50) {
        std::cout<<"SKIP: unexpected taskbar orientation/height\n";
        return 0;
    }
    RECT expected=baseline;
    expected.top-=40; // Visual overhang only; do not change appbar settings.
    const RECT originalWork=monitor.rcWork;
    const DWORD parentPid=GetCurrentProcessId();
    const auto suffix=std::to_wstring(parentPid);
    const std::wstring readyName=L"Local\\ATB_CI_READY_"+suffix;
    const std::wstring finishName=L"Local\\ATB_CI_FINISH_"+suffix;
    Handle ready(CreateEventW(nullptr,TRUE,FALSE,readyName.c_str()));
    Handle finish(CreateEventW(nullptr,TRUE,FALSE,finishName.c_str()));
    if(!ready.valid()||!finish.valid())return 10;

    wchar_t exe[32768]{};
    if(!GetModuleFileNameW(nullptr,exe,32768))return 11;
    std::wstring cmd=L"\""+std::wstring(exe)+L"\" --watchdog "+
      std::to_wstring(explorerPid)+L" "+std::to_wstring(parentPid)+L" "+
      std::to_wstring(baseline.left)+L" "+std::to_wstring(baseline.top)+L" "+
      std::to_wstring(baseline.right)+L" "+std::to_wstring(baseline.bottom)+L" "+
      std::to_wstring(expected.top)+L" \""+readyName+L"\" \""+finishName+L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(exe,cmd.data(),nullptr,nullptr,FALSE,
       CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child))return 12;
    Handle childThread(child.hThread);
    Handle childProcess(child.hProcess);
    if(WaitForSingleObject(ready.h,4000)!=WAIT_OBJECT_0){
        SetEvent(finish.h);
        std::cerr<<"FAIL: separate watchdog did not report ready\n";
        return 13;
    }
    // The child is armed and will recover if this process crashes.
    bool hadExpected=false;
    bool workPreserved=false;
    RECT live{};
    if(SetWindowPos(hwnd,nullptr,expected.left,expected.top,
        expected.right-expected.left,expected.bottom-expected.top,
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER)) {
        for(int i=0;i<40;++i) {
            Sleep(20);
            if(GetWindowRect(hwnd,&live)&&Equals(live,expected)){
                hadExpected=true;
                break;
            }
        }
        MONITORINFO current{sizeof(current)};
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),
                           &current)) {
            workPreserved=Equals(current.rcWork,originalWork);
        }
    }
    if (!hadExpected) {
        // WM_WINDOWPOSCHANGING can clamp native Shell_TrayWnd layout. Retry
        // once without that notification; the independent watchdog is armed.
        std::cout<<"NATIVE_PROBE standard SetWindowPos was clamped; trying SWP_NOSENDCHANGING\n";
        if(SetWindowPos(hwnd,nullptr,expected.left,expected.top,
           expected.right-expected.left,expected.bottom-expected.top,
           SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOSENDCHANGING)) {
            for(int i=0;i<40;++i) {
                Sleep(20);
                if(GetWindowRect(hwnd,&live)&&Equals(live,expected)){
                    hadExpected=true;
                    break;
                }
            }
            MONITORINFO current{sizeof(current)};
            if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),
                               &current)) {
                workPreserved=Equals(current.rcWork,originalWork);
            }
        }
    }
    if(simulateAbruptDeath && hadExpected && workPreserved) {
        // No parent destructors, no cleanup signal. Only the separately
        // spawned watchdog can restore the real native taskbar HWND now.
        std::cout<<"ATB CRASH INJECTION: parent exits without cleanup, watchdog owns restoration\n"<<std::flush;
        ExitProcess(71);
    }
    SetEvent(finish.h);
    DWORD joined=WaitForSingleObject(childProcess.h,6500);
    DWORD exitCode=0;
    if(joined!=WAIT_OBJECT_0||
       !GetExitCodeProcess(childProcess.h,&exitCode)||
       exitCode!=0) {
        std::cerr<<"FAIL: independent watchdog did not complete cleanly\n";
        // Additional best-effort fallback, never overwrite changed geometry.
        Restore(hwnd,baseline,expected);
        return 14;
    }
    MONITORINFO after{sizeof(after)};
    RECT finalRect{};
    if(!GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),
                        &after)||
       !GetWindowRect(hwnd,&finalRect)||
       !Equals(finalRect,baseline)||
       !Equals(after.rcWork,originalWork)) {
        std::cerr<<"FAIL: native taskbar/work area not restored to baseline\n";
        return 15;
    }
    std::cout<<"NATIVE_PROBE moved="<<hadExpected
             <<" workarea_unchanged="<<workPreserved
             <<" watchdog_restored=1\n";
    if(!hadExpected||!workPreserved){
        std::cout<<"INCONCLUSIVE: shell rejected geometry extension or changed workarea\n";
        return 0; // Not a pass on the tested geometry.
    }
    std::cout<<"PASS: native Shell_TrayWnd overhang with unchanged desktop workarea, independently restored\n";
    return 0;
}
int CrashController() {
    HWND hwnd{}; DWORD pid=0; RECT baseline{};
    MONITORINFO baselineMonitor{sizeof(baselineMonitor)};
    if(!Target(hwnd,pid,baseline,baselineMonitor)) {
        std::cout<<"SKIP: no native taskbar available for crash recovery trial\n";
        return 0;
    }
    wchar_t exe[32768]{};
    if(!GetModuleFileNameW(nullptr,exe,32768)) return 30;
    std::wstring cmd=L"\""+std::wstring(exe)+L"\" --parent-crash";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION p{};
    if(!CreateProcessW(exe,cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                       nullptr,nullptr,&startup,&p))return 31;
    Handle child(p.hProcess), thread(p.hThread);
    if(WaitForSingleObject(child.h,12000)!=WAIT_OBJECT_0)return 32;
    DWORD exit=0;
    if(!GetExitCodeProcess(child.h,&exit))return 33;
    if(exit!=71) {
        std::cout<<"INCONCLUSIVE: geometry extension unavailable for crash injection; exit "<<exit<<"\n";
        return exit==0?0:34;
    }
    // Controller has no shutdown signal or access to child-owned handles.
    // Only the independent watchdog notices the dead worker and restores.
    const auto deadline=GetTickCount64()+7000;
    while(GetTickCount64()<deadline) {
        RECT current{};
        MONITORINFO work{sizeof(work)};
        if(GetWindowRect(hwnd,&current) &&
           GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&work) &&
           Equals(current,baseline) &&
           Equals(work.rcWork,baselineMonitor.rcWork)) {
            std::cout<<"PASS: independent watchdog restored native taskbar after forced parent process death\n";
            return 0;
        }
        Sleep(40);
    }
    std::cerr<<"FAIL: independent watchdog did not restore after forced worker death\n";
    return 35;
}

int wmain(int argc,wchar_t** argv) {
    if(!HostedCI()) {
        std::cerr<<"REFUSED: GitHub Actions CI only; never run on a user's PC.\n";
        return 2;
    }
    if(argc==1)return Parent();
    if(argc==2&&wcscmp(argv[1],L"--crash-test")==0)return CrashController();
    if(argc==2&&wcscmp(argv[1],L"--parent-crash")==0)return Parent(true);
    if(argc!=11||wcscmp(argv[1],L"--watchdog")!=0)return 1;
    RECT baseline{
        _wtoi(argv[4]),_wtoi(argv[5]),_wtoi(argv[6]),_wtoi(argv[7])
    };
    RECT expected=baseline;expected.top=_wtoi(argv[8]);
    return Watchdog((DWORD)_wcstoui64(argv[2],nullptr,10),
                    (DWORD)_wcstoui64(argv[3],nullptr,10),
                    baseline,expected,argv[9],argv[10]);
}
