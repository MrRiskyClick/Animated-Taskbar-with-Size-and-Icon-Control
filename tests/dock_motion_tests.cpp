#include "../src/core/dock_motion.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace atb_motion;
static void CheckNoOverlap(const std::vector<Visual>& v, double size, double gap) {
    for (std::size_t i=1;i<v.size();++i) {
        const double previousCenter=(i-1)*(size+gap)+size/2+v[i-1].xShift;
        const double nextCenter=i*(size+gap)+size/2+v[i].xShift;
        const double distance=nextCenter-previousCenter -
            size*(v[i-1].scale+v[i].scale)/2;
        assert(std::fabs(distance-gap)<1e-7);
    }
}
int main() {
    Settings s; std::vector<Icon> icons(12);
    for (double m : {24.0, 80.0, 240.0, 430.0, 900.0, 10.0}) {
        for (int f=0;f<200;++f) {
            auto v=Advance(icons,s,1.0/60,true,m);
            CheckNoOverlap(v,s.iconSize,s.gap);
        }
    }
    s.maxScale=2.5;s.gap=0;s.radius=400;s.spring=100;
    for(int f=0;f<200;++f) {
        auto v=Advance(icons,s,1.0/30,true,(f%2)?700.0:0.0);
        CheckNoOverlap(v,s.iconSize,s.gap);
    }
    for(int f=0;f<400;++f) Advance(icons,s,1.0/60,false,0);
    for(auto& x:icons) assert(std::fabs(x.scale-1.0)<.001);
    assert(PipForWindowCount(0)==Pip::None);
    assert(PipForWindowCount(1)==Pip::One);
    assert(PipForWindowCount(2)==Pip::Two);
    assert(PipForWindowCount(3)==Pip::Three);
    assert(PipForWindowCount(4)==Pip::Bar);
    assert(PipForWindowCount(99)==Pip::Bar);
    std::cout<<"PASS: collision-free spacing, return to idle, pip count mapping\n";
}
