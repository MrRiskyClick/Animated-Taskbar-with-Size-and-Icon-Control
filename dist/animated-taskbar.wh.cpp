// ==WindhawkMod==
// @id animated-taskbar-size-icon-control
// @name Animated Taskbar (Read-Only Lease Diagnostic)
// @description Diagnostic only: taskbar geometry and watchdog heartbeat observation; no shell modifications
// @version 0.0.3
// @author Animated Taskbar project contributors
// @include explorer.exe
// @architecture x86-64
// @compilerOptions -luser32 -ladvapi32
// ==/WindhawkMod==

// ==WindhawkModSettings==
/*
- diagnosticLeaseFile: ""
  $name: Diagnostic lease file (optional)
  $description: Read-only path to a mock watchdog lease.v1 file. Leave blank for no observation.
- diagnosticTrialId: ""
  $name: Expected trial ID (32 lowercase hexadecimal digits)
  $description: Must match the mock lease exactly. This is NOT permission to modify Windows.
- diagnosticPipeSuffix: ""
  $name: Read-only IPC test pipe suffix (optional)
  $description: For a mock-only diagnostic test server. Leave blank on normal installations.
- diagnosticPeerPid: 0
  $name: Expected test server PID
  $description: Diagnostic-only Windows PID; this is not a secure watchdog identity.
- diagnosticPeerExecutable: ""
  $name: Exact test server executable path
  $description: Same-user path check only; NOT a trusted signature or secure IPC.
*/
// ==/WindhawkModSettings==

// ==WindhawkModReadme==
/*
# Stage 1b read-only Windhawk diagnostic
This source **never installs hooks, changes taskbar height, system tray, XAML,
work area, registry values, or other Windhawk mod settings**.
An optional named-pipe probe only verifies PID, same-user/session and the test
executable path and exchanges a static diagnostic acknowledgement. It is NOT
authentication or a trusted lease. Default configuration does not connect.
It can monitor a user-specified `lease.v1` file emitted by the mock watchdog
and log a fail-closed diagnostic result, but the file is NOT authentication.
The session stops at 120 seconds even if the file still looks valid.

Windhawk mods are single-file; use scripts/bundle_windhawk.py to embed our
original src/core headers and native IPC transport before compiling this prototype in Windhawk.
Do not install active taskbar-changing builds until an independent durable
rollback, authenticated IPC and Explorer failure handling are proven.
*/
// ==/WindhawkModReadme==

#include <windows.h>
#include <memory>
#include <string>
#include <utility>

// BEGIN EMBEDDED LEASE PARSER
// Stage-1b original code: strict parser for a read-only watchdog heartbeat.
// Not an authenticated capability, Windows recovery mechanism or permission
// to mutate Explorer. This header is standalone C++17 for testability.

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace animated_taskbar {

struct LeaseV1 {
    std::string trialId;
    std::uint32_t watchdogPid = 0;
    std::uint64_t createdMs = 0;
    std::uint64_t deadlineMs = 0;
    std::uint64_t heartbeatMs = 0;
    std::uint64_t sequence = 0;
};

inline bool ParseUInt64(std::string_view s, std::uint64_t& output) {
    if (s.empty() || s.size() > 20) return false;
    std::uint64_t number = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (number > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return false;
        number = number * 10 + digit;
    }
    output = number;
    return true;
}

inline bool ValidTrialId(std::string_view value) {
    if (value.size() != 32) return false;
    for (char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

inline bool ParseLeaseV1(std::string_view text, LeaseV1& out) {
    // Exact structure; fail closed for extra keys, duplicate keys, CRLF,
    // trailing content, invalid types, overflow and oversized input.
    if (text.empty() || text.size() > 512) return false;
    std::array<std::string_view, 9> lines{};
    for (auto& line : lines) {
        const auto next = text.find('\n');
        if (next == std::string_view::npos) return false;
        line = text.substr(0, next);
        text.remove_prefix(next + 1);
    }
    if (!text.empty() || lines[0] != "ATB_LEASE_V1" ||
        lines[3] != "phase=ACTIVE" || lines[8] != "END") return false;
    constexpr std::string_view prefixId = "id=";
    constexpr std::string_view prefixPid = "pid=";
    constexpr std::string_view prefixCreated = "created_ms=";
    constexpr std::string_view prefixDeadline = "deadline_ms=";
    constexpr std::string_view prefixHeartbeat = "heartbeat_ms=";
    constexpr std::string_view prefixSequence = "sequence=";
    if (lines[1].substr(0, prefixId.size()) != prefixId ||
        lines[2].substr(0, prefixPid.size()) != prefixPid ||
        lines[4].substr(0, prefixCreated.size()) != prefixCreated ||
        lines[5].substr(0, prefixDeadline.size()) != prefixDeadline ||
        lines[6].substr(0, prefixHeartbeat.size()) != prefixHeartbeat ||
        lines[7].substr(0, prefixSequence.size()) != prefixSequence) return false;

    const auto id = lines[1].substr(prefixId.size());
    std::uint64_t pid = 0;
    LeaseV1 parsed;
    if (!ValidTrialId(id) ||
        !ParseUInt64(lines[2].substr(prefixPid.size()), pid) || pid == 0 ||
        pid > std::numeric_limits<std::uint32_t>::max() ||
        !ParseUInt64(lines[4].substr(prefixCreated.size()), parsed.createdMs) ||
        !ParseUInt64(lines[5].substr(prefixDeadline.size()), parsed.deadlineMs) ||
        !ParseUInt64(lines[6].substr(prefixHeartbeat.size()), parsed.heartbeatMs) ||
        !ParseUInt64(lines[7].substr(prefixSequence.size()), parsed.sequence)) {
        return false;
    }
    if (!parsed.createdMs || !parsed.sequence ||
        parsed.deadlineMs <= parsed.createdMs ||
        parsed.deadlineMs - parsed.createdMs > 120000 ||
        parsed.heartbeatMs < parsed.createdMs ||
        parsed.heartbeatMs > parsed.deadlineMs + 2000) return false;
    parsed.trialId = std::string(id);
    parsed.watchdogPid = static_cast<std::uint32_t>(pid);
    out = parsed;
    return true;
}

inline bool LeaseV1IsFresh(const LeaseV1& lease, std::string_view expectedTrial,
                          std::uint64_t nowUtcMs, std::uint64_t maxAgeMs = 1500) {
    // A lease file is an *observer*, not an authorization source. It can only
    // disprove health. No effect-changing Windhawk hook may rely on this alone.
    if (!ValidTrialId(expectedTrial) || lease.trialId != expectedTrial ||
        !maxAgeMs || maxAgeMs > 1500 || nowUtcMs >= lease.deadlineMs ||
        nowUtcMs < lease.createdMs) return false;
    if (lease.heartbeatMs > nowUtcMs + 250) return false;
    // Check using comparison to avoid unsigned subtraction overflow.
    if (nowUtcMs > lease.heartbeatMs &&
        nowUtcMs - lease.heartbeatMs > maxAgeMs) return false;
    return true;
}

class LeaseObserver {
public:
    explicit LeaseObserver(std::string expectedTrial)
        : expectedTrial_(std::move(expectedTrial)) {}

    bool Observe(std::string_view fileData, std::uint64_t nowUtcMs) {
        if (invalid_) return false;
        LeaseV1 lease;
        if (!ParseLeaseV1(fileData, lease) ||
            !LeaseV1IsFresh(lease, expectedTrial_, nowUtcMs) ||
            (lastSequence_ && lease.sequence < lastSequence_) ||
            (lastHeartbeat_ && lease.heartbeatMs < lastHeartbeat_)) {
            invalid_ = true;
            return false;
        }
        lastSequence_ = lease.sequence;
        lastHeartbeat_ = lease.heartbeatMs;
        return true;
    }

    // The only supported way to rearm is a fresh observer object with a new
    // explicit trial ID. Once invalid, this observer is permanently inert.
    bool invalid() const { return invalid_; }

private:
    std::string expectedTrial_;
    std::uint64_t lastSequence_ = 0;
    std::uint64_t lastHeartbeat_ = 0;
    bool invalid_ = false;
};

}  // namespace animated_taskbar

// END EMBEDDED LEASE PARSER
// BEGIN EMBEDDED READ-ONLY IPC CLIENT
// Stage 1g: read-only Windows named-pipe peer DIAGNOSTIC for Windhawk.
// This is not cryptographic authentication, a recovery lease or authorization
// to change the Windows shell. It never writes taskbar/registry/XAML state.
#ifndef _WIN32
#error Windows-only read-only diagnostic
#endif
#include <windows.h>
#include <array>
#include <cstdint>
#include <cwchar>
#include <string>
#include <vector>
// BEGIN EMBEDDED OVERLAPPED IPC TRANSPORT
// Stage 1f: bounded OVERLAPPED named-pipe I/O for MOCK authentication ONLY.
// This is NOT a production timeout guarantee during Windows kernel failure.
// The cancellation drain waits for the kernel to complete the I/O before
// destroying its OVERLAPPED buffer. External CI subprocess timeouts are used.
#ifndef _WIN32
#error Windows-only native pipe transport
#endif
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace atb_transport {

constexpr DWORD kHandshakeTimeoutMs = 3000;
constexpr DWORD kConnectPollMs = 80;

struct Deadline {
    ULONGLONG start = GetTickCount64();
    DWORD budget = kHandshakeTimeoutMs;

    DWORD Remaining() const {
        const ULONGLONG elapsed = GetTickCount64() - start;
        return elapsed >= budget ? 0 : static_cast<DWORD>(budget - elapsed);
    }
};

struct Event {
    HANDLE value = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ~Event() { if (value) CloseHandle(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event() = default;
};

inline bool CompletePending(HANDLE pipe, OVERLAPPED& op, Deadline& deadline,
                            DWORD& bytes) {
    const DWORD remaining = deadline.Remaining();
    if (remaining && GetOverlappedResultEx(pipe, &op, &bytes, remaining, FALSE))
        return true;

    // ERROR_IO_PENDING must be cancelled and DRAINED before the stack-local
    // OVERLAPPED/event/buffer can be freed. CancelIoEx itself is not a wait.
    CancelIoEx(pipe, &op);
    DWORD ignored = 0;
    GetOverlappedResult(pipe, &op, &ignored, TRUE);
    return false;
}

inline bool Connect(HANDLE pipe, Deadline& deadline) {
    if (!deadline.Remaining()) return false;
    Event event;
    if (!event.value) return false;
    OVERLAPPED overlapped{};
    overlapped.hEvent = event.value;
    if (ConnectNamedPipe(pipe, &overlapped)) return true;
    const DWORD error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) return true;
    if (error != ERROR_IO_PENDING) return false;
    DWORD ignored = 0;
    return CompletePending(pipe, overlapped, deadline, ignored);
}

template <std::size_t N>
bool ReadExact(HANDLE pipe, std::array<std::uint8_t, N>& output,
               Deadline& deadline) {
    if (!deadline.Remaining()) return false;
    Event event;
    if (!event.value) return false;
    OVERLAPPED overlapped{};
    overlapped.hEvent = event.value;
    DWORD bytes = 0;
    if (ReadFile(pipe, output.data(), static_cast<DWORD>(N), &bytes, &overlapped))
        return bytes == N;
    if (GetLastError() != ERROR_IO_PENDING) return false;
    return CompletePending(pipe, overlapped, deadline, bytes) && bytes == N;
}

template <std::size_t N>
bool WriteExact(HANDLE pipe, const std::array<std::uint8_t, N>& input,
                Deadline& deadline) {
    if (!deadline.Remaining()) return false;
    Event event;
    if (!event.value) return false;
    OVERLAPPED overlapped{};
    overlapped.hEvent = event.value;
    DWORD bytes = 0;
    if (WriteFile(pipe, input.data(), static_cast<DWORD>(N), &bytes, &overlapped))
        return bytes == N;
    if (GetLastError() != ERROR_IO_PENDING) return false;
    return CompletePending(pipe, overlapped, deadline, bytes) && bytes == N;
}

inline HANDLE OpenClient(const std::wstring& name, Deadline& deadline) {
    while (deadline.Remaining()) {
        HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                  nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) return pipe;
        const DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND)
            break;
        const DWORD remain = deadline.Remaining();
        if (!remain) break;
        const DWORD step = remain < kConnectPollMs ? remain : kConnectPollMs;
        if (error == ERROR_PIPE_BUSY) WaitNamedPipeW(name.c_str(), step);
        else Sleep(step);
    }
    return INVALID_HANDLE_VALUE;
}
} // namespace atb_transport

// END EMBEDDED OVERLAPPED IPC TRANSPORT

namespace atb_readonly {
constexpr std::array<std::uint8_t, 8> kHello{{'A','T','B','D','I','A','G','1'}};
constexpr std::array<std::uint8_t, 8> kReply{{'A','T','B','R','D','Y','0','1'}};

struct ScopedHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle(h) {}
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ~ScopedHandle() {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr) CloseHandle(handle);
    }
    bool valid() const { return handle != INVALID_HANDLE_VALUE && handle != nullptr; }
};

inline bool UserSid(HANDLE token, std::vector<BYTE>& buffer, PSID& sid) {
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size < sizeof(TOKEN_USER) || size > 65536) return false;
    buffer.resize(size);
    DWORD got = 0;
    if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &got) ||
        got < sizeof(TOKEN_USER)) return false;
    sid = reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid;
    return IsValidSid(sid) != FALSE;
}

inline bool ValidSuffix(const std::wstring& suffix) {
    if (suffix.empty() || suffix.size() > 48) return false;
    for (wchar_t c : suffix) {
        if (!((c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-'))
            return false;
    }
    return true;
}

inline bool CheckServerProcess(HANDLE pipe, DWORD expectedPid,
                               const std::wstring& expectedImage) {
    if (!expectedPid || expectedImage.empty() || expectedImage.size() >= 32768)
        return false;
    ULONG realPid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &realPid) ||
        realPid != expectedPid) return false;
    ScopedHandle peer(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, realPid));
    if (!peer.valid()) return false;
    DWORD serverSession = 0, clientSession = 0, status = 0;
    if (!GetExitCodeProcess(peer.handle, &status) || status != STILL_ACTIVE ||
        !ProcessIdToSessionId(realPid, &serverSession) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &clientSession) ||
        serverSession != clientSession) return false;
    ScopedHandle peerToken, ourToken;
    if (!OpenProcessToken(peer.handle, TOKEN_QUERY, &peerToken.handle) ||
        !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ourToken.handle))
        return false;
    std::vector<BYTE> peerMemory, ourMemory;
    PSID peerSid = nullptr, ourSid = nullptr;
    if (!UserSid(peerToken.handle, peerMemory, peerSid) ||
        !UserSid(ourToken.handle, ourMemory, ourSid) ||
        !EqualSid(peerSid, ourSid)) return false;
    std::wstring actualImage(32768, L'\0');
    DWORD size = static_cast<DWORD>(actualImage.size());
    if (!QueryFullProcessImageNameW(peer.handle, 0, actualImage.data(), &size) ||
        size == 0 || size >= 32768) return false;
    actualImage.resize(size);
    // A writable path is NOT a cryptographic binary identity. A subsequent
    // production build must verify publisher/signature/file identity as well.
    return CompareStringOrdinal(actualImage.c_str(), -1, expectedImage.c_str(),
                                -1, TRUE) == CSTR_EQUAL;
}

inline bool Probe(const std::wstring& suffix, DWORD expectedPid,
                  const std::wstring& expectedExe) {
    if (!ValidSuffix(suffix) || !expectedPid || expectedExe.empty())
        return false;
    const std::wstring pipeName = L"\\\\.\\pipe\\ATB-MOCK-IPC-" + suffix;
    atb_transport::Deadline deadline;
    ScopedHandle pipe(atb_transport::OpenClient(pipeName, deadline));
    if (!pipe.valid()) return false;
    if (!CheckServerProcess(pipe.handle, expectedPid, expectedExe)) return false;
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.handle, &mode, nullptr, nullptr))
        return false;
    if (!atb_transport::WriteExact(pipe.handle, kHello, deadline)) return false;
    std::array<std::uint8_t, 8> reply{};
    return atb_transport::ReadExact(pipe.handle, reply, deadline) && reply == kReply;
}
} // namespace atb_readonly

// END EMBEDDED READ-ONLY IPC CLIENT

namespace {
HANDLE g_stopEvent = nullptr;
HANDLE g_timerThread = nullptr;
volatile LONG g_diagnosticInactive = 0;
std::wstring g_leaseFile;
std::unique_ptr<animated_taskbar::LeaseObserver> g_observer;
std::wstring g_ipcSuffix;
std::wstring g_ipcExe;
DWORD g_ipcPid = 0;
bool g_ipcConfigured = false;

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
    if (g_ipcConfigured) {
        // This is read-only peer observation, NEVER trial authorization.
        // Test server identity is based on a same-user path, not a signature.
        const bool observed = atb_readonly::Probe(g_ipcSuffix, g_ipcPid, g_ipcExe);
        if (!observed) {
            InterlockedExchange(&g_diagnosticInactive, 1);
            Wh_Log(L"Diagnostic: read-only IPC peer check rejected; no changes made");
            return 0;
        }
        Wh_Log(L"Diagnostic: read-only IPC peer observed; NOT authenticated");
    }
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

    const PCWSTR configuredSuffix = Wh_GetStringSetting(L"diagnosticPipeSuffix");
    const PCWSTR configuredExe = Wh_GetStringSetting(L"diagnosticPeerExecutable");
    const int configuredPid = Wh_GetIntSetting(L"diagnosticPeerPid");
    std::wstring suffix = configuredSuffix ? configuredSuffix : L"";
    std::wstring exe = configuredExe ? configuredExe : L"";
    if (configuredSuffix) Wh_FreeStringSetting(configuredSuffix);
    if (configuredExe) Wh_FreeStringSetting(configuredExe);
    if (!suffix.empty() || !exe.empty() || configuredPid != 0) {
        if (configuredPid <= 0 || !atb_readonly::ValidSuffix(suffix) || exe.empty()) {
            Wh_Log(L"Diagnostic: incomplete/invalid IPC configuration; refusing");
            return FALSE;
        }
        g_ipcSuffix = std::move(suffix);
        g_ipcExe = std::move(exe);
        g_ipcPid = static_cast<DWORD>(configuredPid);
        g_ipcConfigured = true;
    } else {
        g_ipcConfigured = false;
        g_ipcSuffix.clear();
        g_ipcExe.clear();
        g_ipcPid = 0;
    }

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
    g_ipcSuffix.clear();
    g_ipcExe.clear();
    g_ipcPid = 0;
    g_ipcConfigured = false;
    Wh_Log(L"Diagnostic: unloaded; no taskbar properties were modified");
}
