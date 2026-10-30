// Original, portable animation math for Animated Taskbar.
// No Win32 calls, hooks, shell changes, registry writes, or Windhawk dependencies.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace atb_motion {
struct Settings {
    double iconSize = 48.0, gap = 6.0, radius = 145.0;
    double maxScale = 1.65, spring = 42.0, rise = 18.0;
    bool magnify = true, affectNeighbors = true;
};
struct Icon {
    double scale = 1.0, scaleVelocity = 0.0, lift = 0.0, liftVelocity = 0.0;
};
struct Visual {
    double xShift = 0.0, yShift = 0.0, scale = 1.0;
};
enum class Pip { None, One, Two, Three, Bar };
inline Pip PipForWindowCount(unsigned count) {
    if (!count) return Pip::None;
    if (count == 1) return Pip::One;
    if (count == 2) return Pip::Two;
    if (count == 3) return Pip::Three;
    return Pip::Bar;
}
inline double Clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}
inline std::vector<Visual> Advance(
    std::vector<Icon>& icons, const Settings& cfg, double deltaSeconds,
    bool mousePresent, double mouseX) {
    const double dt = Clamp(deltaSeconds, 0.001, 0.04);
    const double size = std::max(1.0, cfg.iconSize);
    const double gap = std::max(0.0, cfg.gap);
    const double reach = std::max(1.0, cfg.affectNeighbors ? cfg.radius : size * .65);
    const double maxScale = Clamp(cfg.maxScale, 1.0, 2.5);
    const double spring = 15.0 + Clamp(cfg.spring, 0, 100) * .83;
    const double damp = 6.0 + Clamp(cfg.spring, 0, 100) * .19;
    std::vector<double> extras;
    extras.reserve(icons.size());
    double totalExtra = 0;
    for (std::size_t i = 0; i < icons.size(); ++i) {
        auto& a = icons[i];
        const double restingCenter = static_cast<double>(i)*(size+gap)+size/2;
        const double d = mousePresent ? std::fabs(restingCenter-mouseX) : reach;
        const double w = (cfg.magnify && mousePresent) ?
            Clamp(1.0-std::pow(d/reach, 2.0), 0.0, 1.0) : 0.0;
        const double eased = w*w*(3.0-2.0*w);
        const double desired = 1.0+(maxScale-1.0)*eased;
        const double targetLift = std::max(0.0,cfg.rise)*eased;
        a.scaleVelocity = (a.scaleVelocity+(desired-a.scale)*spring*dt)*std::exp(-damp*dt);
        a.scale += a.scaleVelocity*dt;
        a.liftVelocity = (a.liftVelocity+(targetLift-a.lift)*spring*dt)*std::exp(-damp*dt);
        a.lift += a.liftVelocity*dt;
        if (!std::isfinite(a.scale) || !std::isfinite(a.lift)) a = {};
        a.scale = Clamp(a.scale, 1.0, std::min(2.5, maxScale+0.1));
        a.lift = Clamp(a.lift, 0.0, std::max(0.0,cfg.rise)+2.0);
        const double extra = (a.scale-1.0)*size;
        extras.push_back(extra);
        totalExtra += extra;
    }
    double preceding = 0;
    std::vector<Visual> visuals;
    visuals.reserve(icons.size());
    for (std::size_t i = 0; i < icons.size(); ++i) {
        visuals.push_back({preceding+extras[i]/2-totalExtra/2,
                           -icons[i].lift, icons[i].scale});
        preceding += extras[i];
    }
    return visuals;
}
} // namespace atb_motion
