// Copyright (c) Animated Taskbar contributors.
// Pure geometry for a native taskbar's *planned* visual overhang.
// No Windows APIs, no native window resizing, no shell mutations.
#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace atb_regions {
struct Rect {
  double left, top, right, bottom;
  bool contains(double x, double y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }
  double width() const { return std::max(0.0,right-left); }
  double height() const { return std::max(0.0,bottom-top); }
};
struct Monitor {
  Rect physical;  // px, e.g. 0,0,3840,2160
  double dpiScale = 1.0; // XAML DIP -> physical px
};
struct Strip {
  Rect reserved;       // pre-existing Windows taskbar reserved strip, px
  Rect visuals;        // non-mutating *proposed* larger paint/input area, px
  double overhangPx = 0.0;
};
struct Icon {
  double centerXPx;
  double bottomInsetDip;
  double naturalSizeDip;
  double currentScale;    // animated scale
  double liftDip;
};
inline std::optional<Strip> Plan(
    const Monitor& screen,
    double reservedHeightPx,
    double iconSizeDip,
    double maximumScale,
    double maximumLiftDip,
    double bottomInsetDip,
    double safetyDip = 3.0) {
  if (!std::isfinite(screen.dpiScale) || screen.dpiScale <= 0 ||
      !std::isfinite(reservedHeightPx) || reservedHeightPx <= 0 ||
      !std::isfinite(iconSizeDip) || iconSizeDip <= 0 ||
      !std::isfinite(maximumScale) || maximumScale < 1.0 ||
      !std::isfinite(maximumLiftDip) || maximumLiftDip < 0 ||
      !std::isfinite(bottomInsetDip) || bottomInsetDip < 0 ||
      !std::isfinite(safetyDip) || safetyDip < 0 ||
      screen.physical.width() <= 0 || screen.physical.height() <= 0 ||
      reservedHeightPx > screen.physical.height())
      return std::nullopt;
  const auto fullVisualHeightPx = screen.dpiScale *
    (iconSizeDip*maximumScale + maximumLiftDip + bottomInsetDip + safetyDip);
  const double overhang = std::min(
      screen.physical.height()-reservedHeightPx,
      std::max(0.0, std::ceil(fullVisualHeightPx-reservedHeightPx)));
  Strip out{};
  out.reserved = {screen.physical.left,
                  screen.physical.bottom - reservedHeightPx,
                  screen.physical.right, screen.physical.bottom};
  out.overhangPx = overhang;
  out.visuals = {out.reserved.left, out.reserved.top-overhang,
                 out.reserved.right, out.reserved.bottom};
  return out;
}
inline Rect IconBounds(const Strip& bar, const Monitor& monitor, const Icon& i) {
  const double scale = std::max(1.0,i.currentScale);
  const double w = i.naturalSizeDip * scale * monitor.dpiScale;
  const double bottom = bar.reserved.bottom -
    (i.bottomInsetDip + i.liftDip)*monitor.dpiScale;
  return {i.centerXPx-w/2,bottom-w,i.centerXPx+w/2,bottom};
}
// No overhang interaction except over a REAL animated icon. Under the reserved
// strip, native Windows HWND/XAML hit-testing continues to own all input.
inline bool OverhangClaimsPointer(const Strip& bar, const Monitor& screen,
                                  const std::vector<Icon>& icons,
                                  double px, double py) {
  if (!bar.visuals.contains(px,py) || py >= bar.reserved.top) return false;
  for (const auto& icon : icons) {
    if (IconBounds(bar,screen,icon).contains(px,py)) return true;
  }
  return false;
}
} // namespace atb_regions
