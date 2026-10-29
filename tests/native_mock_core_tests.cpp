#include "native/mock_trial_core.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace atb_native_mock;

Trial Sample() {
    Trial t;
    t.id="0123456789abcdef0123456789abcdef";
    t.phase=Phase::active;
    t.baseline=44;
    t.expected=70;
    t.createdUtc=100000;
    t.deadlineUtc=220000;
    t.createdTick=10000;
    t.deadlineTick=130000;
    t.beatUtc=100500;
    t.watcherPid=123;
    return t;
}

int main() {
    Trial t=Sample(), parsed;
    assert(Parse(Serialize(t),parsed));
    assert(parsed.id==t.id && parsed.phase==Phase::active);
    assert(parsed.deadlineUtc==220000 && parsed.baseline==44);
    assert(Fresh(parsed,100600,10500));
    assert(!Fresh(parsed,102100,12000));
    assert(!Fresh(parsed,220000,130000));
    assert(Expired(parsed,220000,12000));
    assert(Expired(parsed,110000,130000));
    assert(Expired(parsed,110000,1)); // simulated boot reset
    assert(!Expired(parsed,100900,12000));
    std::string good=Serialize(t);
    const auto rejects=[&](const std::string& raw) {
        Trial ignored;
        assert(!Parse(raw,ignored));
    };
    rejects(good+"TRAILER");
    rejects(good.substr(0,good.size()-1));
    rejects(std::string(1025,'X'));
    auto bad=good;
    bad.insert(bad.find("phase="),"phase=RECOVERED\n");
    rejects(bad);
    bad=good;
    bad.replace(bad.find("expected=70"),11,"expected=-1");
    rejects(bad);
    bad=good;
    bad.replace(bad.find("deadlineUtc=220000"),18,"deadlineUtc=999999");
    rejects(bad);
    bad=good;
    bad.replace(bad.find("watcherPid=123"),14,"watcherPid=18446744073709551616");
    rejects(bad);
    bad=good;
    bad.replace(bad.find("ACTIVE"),6,"BORKED");
    rejects(bad);
    unsigned h=0;
    assert(ParseTarget(Target(44),h) && h==44);
    assert(!ParseTarget(Target(44)+"oops",h));
    assert(!ParseTarget("ATB_NATIVE_MOCK_TARGET_V1\nheight=999\nEND\n",h));
    assert(!ParseTarget("ATB_NATIVE_MOCK_TARGET_V1\nheight=44\nheight=55\nEND\n",h));

    unsigned current=70; bool changed=false;
    assert(Restore(t,current,changed));
    assert(changed && current==44 && t.phase==Phase::recovered);
    assert(Restore(t,current,changed) && !changed);
    auto conflict=Sample();current=77;
    assert(!Restore(conflict,current,changed));
    assert(current==77 && conflict.phase==Phase::conflict);
    // Repeating recovery on an unresolved conflict must NEVER claim success.
    assert(!Restore(conflict,current,changed));
    assert(current==77 && !changed);
    auto baseline=Sample();current=44;
    assert(Restore(baseline,current,changed));
    assert(!changed && baseline.phase==Phase::recovered);
    std::cout<<"Portable native mock rollback journal contract passed.\n";
}
