// Portable invariant tests for the native two-pass layout formula.
// Tests exactly the displacement relationship used in the Windhawk trial.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>
struct Button { double baseWidth, nativeX, scale, bounce, outputX; };
int main() {
    std::mt19937_64 rng(104729);
    std::uniform_real_distribution<double> width(24, 110);
    std::uniform_real_distribution<double> scaling(1.0, 2.5);
    std::uniform_real_distribution<double> gapDist(0, 18);
    for (int scenario=0; scenario<100000; ++scenario) {
        int n=2+(scenario%29);
        double gap=gapDist(rng);
        std::vector<Button> buttons;
        double center=0;
        for(int i=0;i<n;++i){
            double w=width(rng);
            center += w/2;
            buttons.push_back({w,center,scaling(rng),
                                (i==scenario%n ? 1.05 : 1.0),0.0});
            center += w/2+gap;
        }
        double total=0;
        std::vector<double> extra(n);
        for(int i=0;i<n;++i){
            extra[i]=(buttons[i].scale*buttons[i].bounce-1.0)*buttons[i].baseWidth;
            total+=extra[i];
        }
        double preceding=0;
        for(int i=0;i<n;++i){
            buttons[i].outputX=buttons[i].nativeX + preceding + extra[i]/2 - total/2;
            preceding+=extra[i];
        }
        for(int i=1;i<n;++i){
            double leftEnd=buttons[i-1].outputX+
              buttons[i-1].baseWidth*buttons[i-1].scale*buttons[i-1].bounce/2;
            double rightStart=buttons[i].outputX-
              buttons[i].baseWidth*buttons[i].scale*buttons[i].bounce/2;
            if (std::abs((rightStart-leftEnd)-gap)>1e-8){
                std::cerr<<"FAIL: spacing mismatch at iteration "<<scenario<<"\n";
                return 1;
            }
        }
    }
    std::cout<<"PASS 100,000 heterogeneous-size, bounce-aware icon spacing scenarios\n";
}
