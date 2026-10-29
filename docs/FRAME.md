# 03 — FRAME

This defines the source/ownership boundaries and **non-mutating starting skeleton**.

## Planned tree

```text
README.md
docs/
  PLAN.md
  BLUEPRINT.md
  FRAME.md
  STAGES.md
  UPSTREAM.md
  SAFETY.md
  TESTING.md
  DECISIONS.md
src/
  animated-taskbar.wh.cpp   # Stage-0 diagnostic shell; NO taskbar mutation
  core/trial_guard.hpp      # pure C++ mockable trial rollback contracts
tests/
  trial_guard_tests.cpp     # tests with simulated changes
```

Future modules are proposals, not present:
- `src/windows/taskbar_probe.*` — discovered HWND/XAML and validation
- `src/windows/workarea_controller.*` — native region height/work-area
- `src/windows/native_button_host.*` — task-list/control relocation
- `src/windows/clickthrough.*` — native input hit regions
- `src/windows/animation.*` — magnification, easing and pip transform
- `src/watchdog/*` — **external** recovery agent / durable journal
- `src/windows/journal.*` — snapshot and guarded inverse operations

## Reversible operation contract

Each operation has a name, precondition, captured original state, expected post-state, apply function, inverse/restore function, and verification method. Operations occur only while both watchdog and mod-local guard are healthy. On error/expiry, inverse operations run in reverse order. If restoration cannot be verified, signal error and invoke the *documented safe* escalation instead of silently claiming success.

## Hook contract

1. Never mutate inside `DllMain`.
2. `Wh_ModInit` must abort when preflight fails; `Wh_ModUninit` must attempt restorative cleanup.
3. Every hook must call the original function or return an explicitly validated alternative; no hardcoded undocumented symbol addresses.
4. Store and validate weak XAML references; scope by monitor and DPI; invalidate on Explorer taskbar recreation.
5. Do not change native taskbar alignment, auto-hide, or pinned app data unless required by a separately approved, reversible operation.
6. Do not disable other Windhawk mods automatically; warn of conflict and stop safely.

## Stage 0 implementation boundary

The initial `.wh.cpp` may start a 120-second **diagnostic countdown** and observe native taskbar HWND bounds but **makes no visual, layout, input or work-area changes**. It does NOT satisfy the requirement to auto-disable the installed mod and is NOT the independent watchdog. Do not interpret the countdown as rollback protection.

The pure C++ trial guard is only a proof of rollback semantics. It is neither a Windows snapshot engine nor a production safety system.

## Build/test workflow

- Run native-independent C++17 rollback contract unit tests using a local compiler.
- Compile `.wh.cpp` with Windhawk's mod builder on a non-critical test configuration only.
- User visually checks hover, stacking, Aero Peek, clip boundaries, click-through and monitor behavior; agent may inspect code and build, but must not claim visual verification based on screenshots alone.
- Do not automatically install, run or enable the mod during development.
