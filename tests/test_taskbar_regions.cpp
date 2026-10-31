#include "../src/core/taskbar_regions.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using namespace atb_regions;
int main() {
  unsigned tested=0;
  for (double scale : {1.0,1.25,1.5,1.75,2.0,2.5}) {
    for (double h : {36.0,42.0,48.0,54.0,64.0,82.0}) {
      Monitor m{{0,0,3840,2160},scale};
      auto result=Plan(m,h,36.0,2.2,16.0,3.0);
      assert(result);
      const auto& b=*result;
      // The native system reservation NEVER grows with the visual extension.
      assert(b.reserved.top == 2160.0-h);
      assert(b.reserved.height() == h);
      assert(b.visuals.top <= b.reserved.top);
      assert(b.visuals.bottom == b.reserved.bottom);
      assert(b.visuals.top >= m.physical.top);
      Icon icon{1920.0,3.0,36.0,2.2,16.0};
      const auto bbox=IconBounds(b,m,icon);
      assert(bbox.top>=b.visuals.top-1e-8);
      // Overhang icons are interactive, remaining transparent pixels pass.
      double y=std::max(bbox.top,b.visuals.top)+0.05;
      if (y<b.reserved.top) {
         assert(OverhangClaimsPointer(b,m,{icon},1920.0,y));
         assert(!OverhangClaimsPointer(b,m,{icon},50.0,y));
      }
      assert(!OverhangClaimsPointer(b,m,{icon},1920.0,2159.0));
      ++tested;
    }
  }
  assert(!Plan({{0,0,3840,2160},0},42,36,2,8,0));
  assert(!Plan({{0,0,3840,2160},1},0,36,2,8,0));
  assert(!Plan({{0,0,3840,2160},1},42,36,0.5,8,0));
  std::cout<<"PASS: "<<tested<<" reserved-strip, overhang, DPI and hit-test layouts\n";
}
