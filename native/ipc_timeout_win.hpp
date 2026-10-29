// Stage 1f: bounded OVERLAPPED named-pipe I/O for MOCK authentication ONLY.
// This is NOT a production timeout guarantee during Windows kernel failure.
// The cancellation drain waits for the kernel to complete the I/O before
// destroying its OVERLAPPED buffer. External CI subprocess timeouts are used.
#pragma once
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
