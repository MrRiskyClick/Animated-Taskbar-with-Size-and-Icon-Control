// Stage 1j: portable, deterministic journal for an isolated mock-file trial.
// NOT Windows taskbar geometry, an Explorer recovery mechanism or authorization.
#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace atb_native_mock {
constexpr std::uint64_t kProductionDeadlineMs = 120000;
constexpr std::uint64_t kMaxHeartbeatAgeMs = 1500;
constexpr std::string_view kJournalMarker = "ATB_NATIVE_MOCK_JOURNAL_V1";
constexpr std::string_view kTargetMarker = "ATB_NATIVE_MOCK_TARGET_V1";

enum class Phase { prepared, ready, active, recovered, conflict };
struct Trial {
    std::string id;
    Phase phase = Phase::prepared;
    unsigned baseline = 0, expected = 0;
    std::uint64_t createdUtc = 0, deadlineUtc = 0;
    std::uint64_t createdTick = 0, deadlineTick = 0;
    std::uint64_t beatUtc = 0;
    std::uint64_t watcherPid = 0;
};

inline bool Number(std::string_view s, std::uint64_t& out) {
    if (s.empty() || s.size() > 20) return false;
    std::uint64_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        const auto x = static_cast<std::uint64_t>(c - '0');
        if (n > (std::numeric_limits<std::uint64_t>::max() - x) / 10) return false;
        n = n * 10 + x;
    }
    out = n; return true;
}
inline bool Height(std::uint64_t n) { return n >= 16 && n <= 160; }
inline bool ValidId(std::string_view id) {
    if (id.size() != 32) return false;
    for (char c : id)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
inline std::string_view Name(Phase p) {
    switch(p) {
        case Phase::prepared: return "PREPARED";
        case Phase::ready: return "READY";
        case Phase::active: return "ACTIVE";
        case Phase::recovered: return "RECOVERED";
        case Phase::conflict: return "CONFLICT";
    }
    return "INVALID";
}
inline bool ParsePhase(std::string_view s, Phase& out) {
    for (auto p : {Phase::prepared,Phase::ready,Phase::active,
                   Phase::recovered,Phase::conflict}) {
        if (s == Name(p)) { out = p; return true; }
    }
    return false;
}
template<std::size_t N>
inline bool Lines(std::string_view data, std::array<std::string_view,N>& lines) {
    if (data.size() > 1024) return false;
    for (auto& s : lines) {
        const auto n = data.find('\n');
        if (n == std::string_view::npos) return false;
        s = data.substr(0,n);
        data.remove_prefix(n+1);
    }
    return data.empty();
}
inline bool HasPrefix(std::string_view line, std::string_view prefix,
                      std::string_view& out) {
    if (line.substr(0,prefix.size()) != prefix) return false;
    out = line.substr(prefix.size()); return true;
}
inline bool ValidTrial(const Trial& t) {
    if (!ValidId(t.id) || !Height(t.baseline) || !Height(t.expected) ||
        !t.createdUtc || !t.createdTick ||
        t.deadlineUtc <= t.createdUtc ||
        t.deadlineTick <= t.createdTick ||
        t.deadlineUtc - t.createdUtc > kProductionDeadlineMs ||
        t.deadlineTick - t.createdTick > kProductionDeadlineMs) return false;
    if (t.phase == Phase::ready || t.phase == Phase::active) {
        if (!t.watcherPid || t.beatUtc < t.createdUtc) return false;
    }
    return true;
}
inline std::string Serialize(const Trial& t) {
    return std::string(kJournalMarker) + "\n" +
           "id=" + t.id + "\nphase=" + std::string(Name(t.phase)) +
           "\nbaseline=" + std::to_string(t.baseline) +
           "\nexpected=" + std::to_string(t.expected) +
           "\ncreatedUtc=" + std::to_string(t.createdUtc) +
           "\ndeadlineUtc=" + std::to_string(t.deadlineUtc) +
           "\ncreatedTick=" + std::to_string(t.createdTick) +
           "\ndeadlineTick=" + std::to_string(t.deadlineTick) +
           "\nbeatUtc=" + std::to_string(t.beatUtc) +
           "\nwatcherPid=" + std::to_string(t.watcherPid) + "\nEND\n";
}
inline bool Parse(std::string_view data, Trial& out) {
    std::array<std::string_view,12> l{};
    if (!Lines(data,l) || l[0] != kJournalMarker || l[11] != "END") return false;
    std::array<std::string_view,10> fields{};
    constexpr std::array<std::string_view,10> prefixes{
        "id=","phase=","baseline=","expected=",
        "createdUtc=","deadlineUtc=","createdTick=","deadlineTick=",
        "beatUtc=","watcherPid="};
    for (std::size_t i=0; i<fields.size(); ++i)
        if (!HasPrefix(l[i+1],prefixes[i],fields[i])) return false;
    Trial t;
    t.id = std::string(fields[0]);
    if (!ParsePhase(fields[1],t.phase)) return false;
    std::uint64_t base=0,expected=0;
    if (!Number(fields[2],base) || !Height(base) ||
        !Number(fields[3],expected) || !Height(expected) ||
        !Number(fields[4],t.createdUtc) || !Number(fields[5],t.deadlineUtc) ||
        !Number(fields[6],t.createdTick) || !Number(fields[7],t.deadlineTick) ||
        !Number(fields[8],t.beatUtc) || !Number(fields[9],t.watcherPid))
        return false;
    t.baseline=static_cast<unsigned>(base);
    t.expected=static_cast<unsigned>(expected);
    if (!ValidTrial(t)) return false;
    out = std::move(t); return true;
}
inline std::string Target(unsigned h) {
    return std::string(kTargetMarker)+"\nheight="+std::to_string(h)+"\nEND\n";
}
inline bool ParseTarget(std::string_view data, unsigned& h) {
    std::array<std::string_view,3> l{};
    if (!Lines(data,l) || l[0] != kTargetMarker || l[2] != "END") return false;
    std::string_view s;
    std::uint64_t n=0;
    if (!HasPrefix(l[1],"height=",s) || !Number(s,n) || !Height(n)) return false;
    h=static_cast<unsigned>(n); return true;
}
inline bool Expired(const Trial& t, std::uint64_t utc, std::uint64_t tick) {
    return utc >= t.deadlineUtc || tick >= t.deadlineTick ||
           tick < t.createdTick || utc < t.createdUtc;
}
inline bool Fresh(const Trial& t, std::uint64_t utc, std::uint64_t tick) {
    return (t.phase == Phase::ready || t.phase == Phase::active) &&
           !Expired(t,utc,tick) && t.beatUtc <= utc &&
           utc-t.beatUtc <= kMaxHeartbeatAgeMs;
}
// Pure recovery contract: never overwrite a third-party value.
inline bool Restore(Trial& t, unsigned& current, bool& changed) {
    changed = false;
    if (t.phase == Phase::recovered || t.phase == Phase::conflict) return true;
    if (current == t.expected) {
        current = t.baseline;
        changed = true;
        t.phase = Phase::recovered;
        return true;
    }
    if (current == t.baseline) {
        t.phase = Phase::recovered; return true;
    }
    t.phase = Phase::conflict; return false;
}
} // namespace atb_native_mock
