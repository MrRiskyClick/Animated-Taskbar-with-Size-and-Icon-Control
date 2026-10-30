# Native taskbar inventory: next safe integration test

This stage is **not the complete animated taskbar mod**. It is a genuine native
`Taskbar.View.dll` symbol hook that only *reads* taskbar XAML properties.
It supersedes geometry-only diagnostics for native control discovery.

## Why it matters
The approved design needs task buttons extending beyond the reserved 42-pixel
system taskbar without clipping, and neighbouring icon spacing that remains
collision-free throughout animation. Win32 taskbar rectangles alone cannot
identify which nested XAML controls restrict drawing or interaction.

## Test on the PC (no visual changes expected)
1. Disable/remove the previous `animated-taskbar-size-icon-control` diagnostic.
2. Create a **new** Windhawk local mod and paste the entire
   `experiments/atb-native-inventory.wh.cpp` source.
3. Compile, enable Detailed debug logs, and enable the mod.
4. Hover the taskbar once. Look for `[ATB INVENTORY]` lines including
   `nativeTaskButtons`, each Taskbar class size, `clipRect`, and the work area.
5. Send the text log, not screenshots; no actual animation or changes should occur.
6. Disable/remove the inventory mod after the readout.

The symbol is derived from Ph0en1x-dev's Taskbar Dock Animation 1.9.2,
but the inspection traversal is original. Upstream source:
https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-dock-animation.wh.cpp

### Guardrails
- Bounded at 600 nodes and 95 log entries, and one snapshot per injection.
- Stops logging after 120 seconds, though no independent recovery is necessary
  because no XAML property writes are performed.
- No persisted state, no taskbar background/size changes, and no change to other mods.
- If Windows changes the XAML symbol and hooking fails, this build fails rather
  than modifying the taskbar.

## Next implementation milestones
1. Confirm native XAML clipping nodes and identity of real taskbar buttons on the
   user's Windows build, including secondary monitors if present.
2. Preserve/restorable RenderTransform and Clip values without stealing ownership
   from the user's Taskbar Styler theme.
3. Implement real visual-only native magnification from `src/core/dock_motion.hpp`,
   ensuring positions derive from the **current** scale values each frame.
4. Build authenticated independently supervised 120-second arming, durable
   transaction journal, fail-closed on loss/restart, and Windows failure injection.
5. Only after the active-trial safety gate passes, connect higher-risk taskbar
   work-area/height, click-through, and custom native pip rendering.
