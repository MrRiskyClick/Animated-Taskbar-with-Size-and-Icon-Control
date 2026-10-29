// STAGE 1j: Windows-native, MOCK-FILE-ONLY watchdog executable.
// No Explorer calls, Windhawk control, taskbar geometry or registry writes.
// Built as "atb-native-mock-watchdog.exe", NOT the signed production watchdog.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "native/mock_trial_core.hpp"
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
using atb_native_mock::Trial;
using atb_native_mock::Phase;

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE v = INVALID_HANDLE_VALUE) : h(v) {}
    ~Handle() { if (h != INVALID_HANDLE_VALUE && h != nullptr) CloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    bool valid() const { return h != INVALID_HANDLE_VALUE && h != nullptr; }
};

std::wstring Utf8(const char* text) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          text, -1, nullptr, 0);
    if (count <= 1 || count > 32768) return {};
    std::wstring value(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                             value.data(), count)) return {};
    value.resize(static_cast<std::size_t>(count-1));
    return value;
}
bool BeginsInsensitive(const std::wstring& path, const std::wstring& prefix) {
    if (path.size() < prefix.size()) return false;
    return CompareStringOrdinal(path.data(), static_cast<int>(prefix.size()),
                                prefix.data(), static_cast<int>(prefix.size()),
                                TRUE) == CSTR_EQUAL;
}
std::wstring Absolute(const std::wstring& path) {
    DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!needed || needed >= 32768) return {};
    std::wstring buffer(needed, L'\0');
    if (!GetFullPathNameW(path.c_str(), needed, buffer.data(), nullptr))
        return {};
    buffer.resize(wcslen(buffer.c_str()));
    return buffer;
}
std::wstring AllowedRoot(const char* raw) {
    const std::wstring requested = Utf8(raw);
    if (requested.empty()) return {};
    wchar_t temp[MAX_PATH+1]{};
    DWORD length = GetTempPathW(MAX_PATH, temp);
    if (!length || length >= MAX_PATH) return {};
    const std::wstring prefix = Absolute(temp);
    const std::wstring path = Absolute(requested);
    if (prefix.empty() || path.empty()) return {};
    std::wstring prefixWithSlash = prefix;
    if (prefixWithSlash.back() != L'\\') prefixWithSlash += L'\\';
    if (!BeginsInsensitive(path, prefixWithSlash)) return {};
    // Restrict all writes to a single explicitly marked TEMP directory.
    const std::wstring name = path.substr(prefixWithSlash.size());
    constexpr wchar_t marker[] = L"atb-native-mock-";
    if (name.rfind(marker,0) != 0 || name.size() <= wcslen(marker) ||
        name.size() > 80 || name.find(L'\\') != std::wstring::npos ||
        name.find(L'/') != std::wstring::npos) return {};
    for (wchar_t ch : name)
        if (!((ch >= L'a' && ch <= L'z') ||
              (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_')) return {};
    return path;
}
bool ValidDirectory(const std::wstring& root) {
    DWORD attr = GetFileAttributesW(root.c_str());
    return attr != INVALID_FILE_ATTRIBUTES &&
           (attr & FILE_ATTRIBUTE_DIRECTORY) &&
           !(attr & FILE_ATTRIBUTE_REPARSE_POINT);
}
std::wstring File(const std::wstring& root, const wchar_t* name) {
    return root + L"\\" + name;
}
std::uint64_t UtcMs() {
    FILETIME f{};
    GetSystemTimeAsFileTime(&f);
    ULARGE_INTEGER n{};
    n.LowPart=f.dwLowDateTime; n.HighPart=f.dwHighDateTime;
    constexpr std::uint64_t kEpoch = 116444736000000000ULL;
    return (n.QuadPart - kEpoch) / 10000;
}
std::uint64_t TickMs() { return GetTickCount64(); }
bool TestMode() {
    wchar_t value[8]{};
    return GetEnvironmentVariableW(L"ATB_NATIVE_MOCK_TEST_MODE", value, 8) == 1 &&
           value[0] == L'1';
}
bool ParseSmall(const char* arg, unsigned& out) {
    std::uint64_t number=0;
    if (!atb_native_mock::Number(arg,number) ||
        number > 1000000) return false;
    out=static_cast<unsigned>(number); return true;
}
bool GenerateId(std::string& out) {
    unsigned char random[16]{};
    if (BCryptGenRandom(nullptr, random, 16,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
    constexpr char hx[]="0123456789abcdef";
    out.clear(); out.reserve(32);
    for (unsigned char b : random) {
        out += hx[b>>4]; out += hx[b&15];
    }
    return true;
}
bool ReadSmall(const std::wstring& path, std::string& out) {
    Handle f(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!f.valid()) return false;
    char data[1025]{};
    DWORD read=0;
    if (!ReadFile(f.h, data, sizeof(data), &read, nullptr) ||
        !read || read > 1024) return false;
    out.assign(data,data+read); return true;
}
// Exclusive lock is released automatically by the kernel on process death.
// Never delete the lock file, avoiding lock-inode replacement races.
struct Lock {
    Handle file;
    explicit Lock(const std::wstring& root) {
        const std::wstring path = File(root,L"state.lock");
        const ULONGLONG started=TickMs();
        while (TickMs()-started < 2000) {
            file.h=CreateFileW(path.c_str(), GENERIC_READ|GENERIC_WRITE,
                               0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if (file.valid()) return;
            if (GetLastError() != ERROR_SHARING_VIOLATION) return;
            Sleep(20);
        }
    }
    bool valid() const { return file.valid(); }
};
bool WriteAtomic(const std::wstring& dest, const std::string& data) {
    if (data.empty() || data.size()>1024) return false;
    std::wstring temp=dest+L".tmp-"+std::to_wstring(GetCurrentProcessId())+
                      L"-"+std::to_wstring(TickMs());
    Handle f(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
                          nullptr));
    if (!f.valid()) return false;
    DWORD written=0;
    bool ok=WriteFile(f.h,data.data(),static_cast<DWORD>(data.size()),
                       &written,nullptr) && written == data.size() &&
            FlushFileBuffers(f.h);
    // Close before replacing: Windows can reject MoveFileEx on an open source.
    CloseHandle(f.h); f.h=INVALID_HANDLE_VALUE;
    if (ok) ok=MoveFileExW(temp.c_str(),dest.c_str(),
                          MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if (!ok) DeleteFileW(temp.c_str());
    return ok;
}
bool Load(const std::wstring& root, Trial& t) {
    std::string input;
    return ReadSmall(File(root,L"journal.v1"),input) &&
           atb_native_mock::Parse(input,t);
}
bool LoadTarget(const std::wstring& root, unsigned& height) {
    std::string data;
    return ReadSmall(File(root,L"target.v1"),data) &&
           atb_native_mock::ParseTarget(data,height);
}
bool SaveTrial(const std::wstring& root,const Trial& t) {
    return atb_native_mock::ValidTrial(t) &&
           WriteAtomic(File(root,L"journal.v1"),atb_native_mock::Serialize(t));
}
bool SaveTarget(const std::wstring& root,unsigned height) {
    return atb_native_mock::Height(height) &&
           WriteAtomic(File(root,L"target.v1"),atb_native_mock::Target(height));
}
int RecoverLocked(const std::wstring& root,Trial& t, const char* reason) {
    unsigned current=0;
    if (!LoadTarget(root,current)) return 3;
    bool changed=false;
    const bool noConflict=atb_native_mock::Restore(t,current,changed);
    // Restore the fixture BEFORE marking journal terminal. On crash, a
    // repeated reconciliation can complete without hiding unfinished work.
    if (changed && !SaveTarget(root,current)) return 3;
    if (!SaveTrial(root,t)) return 3;
    std::cout << "PHASE=" << atb_native_mock::Name(t.phase)
              << " REASON=" << reason << "\n";
    return noConflict ? 0 : 3;
}
bool WatcherAlive(const Trial& t) {
    if (!t.watcherPid || t.watcherPid > 0xffffffffULL) return false;
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                               FALSE,static_cast<DWORD>(t.watcherPid)));
    DWORD code=0;
    return process.valid() && GetExitCodeProcess(process.h,&code) &&
           code == STILL_ACTIVE;
}
int Init(const std::wstring& root,unsigned height) {
    if (!atb_native_mock::Height(height)) return 2;
    if (!CreateDirectoryW(root.c_str(),nullptr) &&
        GetLastError()!=ERROR_ALREADY_EXISTS) return 3;
    if (!ValidDirectory(root)) return 3;
    Lock lock(root);
    if (!lock.valid()) return 3;
    if (GetFileAttributesW(File(root,L"journal.v1").c_str())!=INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(File(root,L"target.v1").c_str())!=INVALID_FILE_ATTRIBUTES)
        return 3;
    if (!SaveTarget(root,height)) return 3;
    std::cout << "MOCK_INITIALIZED\n";return 0;
}
int Arm(const std::wstring& root,unsigned expected,unsigned seconds) {
    if (!atb_native_mock::Height(expected) || !seconds || seconds>120 ||
        (seconds != 120 && !TestMode())) return 2;
    Lock lock(root);if (!lock.valid()) return 3;
    unsigned baseline=0;
    if (!LoadTarget(root,baseline)) return 3;
    const std::wstring path=File(root,L"journal.v1");
    if (GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) {
        Trial prior;
        if (!Load(root,prior) ||
            (prior.phase != Phase::recovered)) return 3;
    }
    Trial t;
    if (!GenerateId(t.id)) return 3;
    t.phase=Phase::prepared;
    t.baseline=baseline;t.expected=expected;
    t.createdUtc=UtcMs();t.createdTick=TickMs();
    t.deadlineUtc=t.createdUtc+static_cast<std::uint64_t>(seconds)*1000;
    t.deadlineTick=t.createdTick+static_cast<std::uint64_t>(seconds)*1000;
    if (!SaveTrial(root,t)) return 3;
    std::cout << "TRIAL=" << t.id << "\n";return 0;
}
int Run(const std::wstring& root,const std::string& id) {
    if (!atb_native_mock::ValidId(id)) return 2;
    {
        Lock lock(root);if (!lock.valid()) return 3;
        Trial t;
        if (!Load(root,t) || t.id!=id || t.phase!=Phase::prepared) return 3;
        if (atb_native_mock::Expired(t,UtcMs(),TickMs()))
            return RecoverLocked(root,t,"deadline-before-start");
        t.phase=Phase::ready;t.watcherPid=GetCurrentProcessId();t.beatUtc=UtcMs();
        if (!SaveTrial(root,t)) return 3;
        std::cout << "READY\n" << std::flush;
    }
    while (true) {
        Sleep(100);
        Lock lock(root);if (!lock.valid()) return 3;
        Trial t;
        if (!Load(root,t) || t.id!=id || t.watcherPid!=GetCurrentProcessId())
            return 3;
        if (t.phase==Phase::recovered) return 0;
        if (t.phase==Phase::conflict) return 3;
        if (t.phase!=Phase::ready && t.phase!=Phase::active) return 3;
        if (atb_native_mock::Expired(t,UtcMs(),TickMs()))
            return RecoverLocked(root,t,"deadline");
        t.beatUtc=UtcMs();
        if (!SaveTrial(root,t)) return 3;
    }
}
int Apply(const std::wstring& root,const std::string& id) {
    Lock lock(root);if(!lock.valid()) return 3;
    Trial t;
    if (!atb_native_mock::ValidId(id) || !Load(root,t) || t.id!=id ||
        t.phase!=Phase::ready ||
        !atb_native_mock::Fresh(t,UtcMs(),TickMs()) || !WatcherAlive(t))
        return 3;
    unsigned current=0;
    if (!LoadTarget(root,current) || current!=t.baseline) return 3;
    if (!SaveTarget(root,t.expected)) return 3;
    // On a crash here, a READY journal still knows baseline and expected;
    // its watcher or a later explicit recovery can restore the mock.
    t.phase=Phase::active;
    if (!SaveTrial(root,t)) return 3;
    std::cout << "MOCK_APPLIED\n";return 0;
}
int Status(const std::wstring& root) {
    Lock lock(root);if(!lock.valid()) return 3;
    Trial t;unsigned h=0;
    if (!Load(root,t) || !LoadTarget(root,h)) return 3;
    std::cout << "PHASE=" << atb_native_mock::Name(t.phase)
              << " HEIGHT=" << h << " TRIAL=" << t.id << "\n";return 0;
}
int Recover(const std::wstring& root,bool reconciliation) {
    Lock lock(root);if(!lock.valid()) return 3;
    Trial t;
    if (!Load(root,t)) return 3;
    if (reconciliation) {
        if (t.phase==Phase::recovered) return 0;
        if (t.phase==Phase::conflict) return 3;
        const auto now=UtcMs();
        if (!atb_native_mock::Expired(t,now,TickMs())) {
            if (t.phase==Phase::ready || t.phase==Phase::active) {
                if (t.beatUtc <= now &&
                    now-t.beatUtc <= atb_native_mock::kMaxHeartbeatAgeMs)
                    return 3; // live trial may not be silently interrupted
            } else return 3;
        }
    }
    return RecoverLocked(root,t,reconciliation?"reconcile":"manual");
}
} // namespace
int main(int argc,char** argv) {
    if (argc<3) return 2;
    const std::wstring root=AllowedRoot(argv[2]);
    if (root.empty()) return 2;
    const std::string command(argv[1]);
    if (command=="init" && argc==4) {
        unsigned height=0;
        return ParseSmall(argv[3],height) ? Init(root,height) : 2;
    }
    if (!ValidDirectory(root)) return 3;
    if (command=="arm" && argc==5) {
        unsigned height=0,seconds=0;
        return ParseSmall(argv[3],height) && ParseSmall(argv[4],seconds)
               ? Arm(root,height,seconds) : 2;
    }
    if (command=="run" && argc==4) return Run(root,argv[3]);
    if (command=="apply" && argc==4) return Apply(root,argv[3]);
    if (command=="status" && argc==3) return Status(root);
    if (command=="recover" && argc==3) return Recover(root,false);
    if (command=="reconcile" && argc==3) return Recover(root,true);
    return 2;
}
