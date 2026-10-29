// Stage-1b original code: strict parser for a read-only watchdog heartbeat.
// Not an authenticated capability, Windows recovery mechanism or permission
// to mutate Explorer. This header is standalone C++17 for testability.
#pragma once

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
