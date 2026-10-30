#include "src/core/lease_v1.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

using animated_taskbar::LeaseV1;
using animated_taskbar::ParseLeaseV1;
using animated_taskbar::LeaseV1IsFresh;
using animated_taskbar::LeaseObserver;

constexpr const char* kId = "0123456789abcdef0123456789abcdef";

std::string Lease(std::uint64_t heartbeat = 100500, unsigned sequence = 1) {
    return "ATB_LEASE_V1\nid=" + std::string(kId) +
           "\npid=123\nphase=ACTIVE\ncreated_ms=100000\n"
           "deadline_ms=220000\nheartbeat_ms=" +
           std::to_string(heartbeat) + "\nsequence=" +
           std::to_string(sequence) + "\nEND\n";
}

int main() {
    const auto healthy = Lease();
    LeaseV1 value;
    assert(ParseLeaseV1(healthy, value));
    assert(value.watchdogPid == 123 && value.deadlineMs == 220000);
    assert(value.sequence == 1 && value.trialId == kId);
    assert(LeaseV1IsFresh(value, kId, 100600));
    assert(!LeaseV1IsFresh(value, "ffffffffffffffffffffffffffffffff", 100600));
    assert(!LeaseV1IsFresh(value, kId, 220000));
    assert(!LeaseV1IsFresh(value, kId, 102100)); // stale heartbeat
    assert(!LeaseV1IsFresh(value, kId, 100100)); // future heartbeat
    assert(!LeaseV1IsFresh(value, kId, 100600, 5000)); // invalid age limit

    const auto rejects = [&](std::string s) {
        LeaseV1 ignored;
        assert(!ParseLeaseV1(s, ignored));
    };
    rejects(healthy + "anything");
    rejects(healthy.substr(0, healthy.size() - 1));
    rejects(std::string(700, 'a'));
    rejects("ATB_LEASE_V1\nid=x\npid=123\nphase=ACTIVE\ncreated_ms=100000\n"
            "deadline_ms=220000\nheartbeat_ms=100500\nsequence=1\nEND\n");
    auto corrupt = healthy;
    corrupt.replace(corrupt.find("ACTIVE"), 6, "EXPIRE");
    rejects(corrupt);
    corrupt = healthy;
    corrupt.replace(corrupt.find("220000"), 6, "999999");
    rejects(corrupt);
    corrupt = healthy;
    corrupt.replace(corrupt.find("pid=123"), 7, "pid=-23");
    rejects(corrupt);
    corrupt = healthy;
    corrupt.replace(corrupt.find("sequence=1"), 10, "sequence=18446744073709551616");
    rejects(corrupt);
    corrupt = healthy;
    corrupt.replace(corrupt.find("created_ms=100000"), 17, "created_ms=999999");
    rejects(corrupt);
    corrupt = healthy;
    corrupt.insert(corrupt.find("pid=123"), "id=overridden\n");
    rejects(corrupt);
    corrupt = healthy;
    corrupt.replace(corrupt.find("id=012"), 6, "id=XYZ");
    rejects(corrupt);

    LeaseObserver observer(kId);
    assert(observer.Observe(Lease(100500, 1), 100600));
    assert(observer.Observe(Lease(100800, 2), 100900));
    assert(!observer.Observe(Lease(100500, 1), 100900)); // regression latches
    assert(observer.invalid());
    assert(!observer.Observe(Lease(101000, 3), 101100)); // cannot rearm

    LeaseObserver badId("deadbeefdeadbeefdeadbeefdeadbeef");
    assert(!badId.Observe(Lease(), 100600));
    assert(badId.invalid());

    LeaseObserver stale(kId);
    assert(!stale.Observe(Lease(), 120000));
    assert(!stale.Observe(Lease(120000, 3), 120100));

    std::cout << "C++ lease parser/observer contract passed (read-only).\n";
}
