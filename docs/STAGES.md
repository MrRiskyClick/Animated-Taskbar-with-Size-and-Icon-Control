# 04 — STAGES & GATES

Stages are sequential: passing a stage requires evidence, not just compiling.

| Stage | Deliverable | Release gate |
|---|---|---|
| 0 — Research + inert scaffold | Docs, upstream source map, original mockable rollback core, non-mutating Windhawk probe | Static inspection, mock tests pass; no shell mutation |
| 1 — Safety gate | Independent watchdog, durable journal, 120s TTL, expiry/disarm and Explorer-restart recovery | **Failure injection** passes before any active taskbar mutation |
| 2 — Native taskbar inventory | Read-only HWND/XAML/monitor inventory, OS/build/symbol detection | Unknown build fails closed; no wrong-monitor handles |
| 3 — Reserved geometry | Reversible Taskbar 1 height and native work-area reservation | Maximized work-area edge stays at configured reserved bar; safe rollback |
| 4 — Native visual extension | Native task buttons render in taller unreserved region, tray remains native | No clipping; grouping, hover previews, Jump Lists still function |
| 5 — Interaction | Hit-test empty transparent region / icon buttons / tooltips / menus | Click-through and mouse/keyboard native behavior tested |
| 6 — Customization | Width, size, spacing, pip alignment, offset, toggle, hover magnification | Visual + behavior tested; no regression on expiry |
| 7 — Compatibility & packaging | Multi-monitor, DPI, Explorer restart, fullscreen, coexisting mods, docs and release | Test matrix complete; safe rollback on unknown OS/failed symbol |

### Stage 1 failure-injection checklist (must pass before Stage 3)

- Ordinary 120-second expiry.
- Loss of heartbeat / missed deadline.
- Explorer crash and restart.
- Explorer frozen / blocked callback.
- Loss of watchdog process, crash and machine reboot while armed.
- Secondary monitor disconnect and DPI change during trial.
- Unexpected mod unload, Windhawk restart, Windows update changing symbols.
- Reverse-order restoration when an intermediate operation fails.
- User modification of a value during trial (do not overwrite unexpectedly).
- Escalation path has an explicit bounded timeout; does not endlessly restart Explorer.

### Success criteria — native behavior

- Taskbar system tray and date/time remain visible and usable.
- Pinned/running icons, order, app grouping and taskbar activation continue to reflect shell state.
- Aero Peek/thumbnail preview, context menus/Jump Lists, active application indicator, drag-and-drop and keyboard interactions are explicitly checked.
- Dock icons magnify without crossing a crop rectangle; no extra maximized-window reservation; clicks in transparent free space reach underlying app.
- When trial expires, the taskbar returns to its **recorded pre-trial baseline**, and the mod cannot silently reapply.

### Stage 0 status

Documentation/scaffold only. No claims about successful Windhawk compilation, native rehosting, watchdog readiness, or visual behavior until independently demonstrated.

## Expanded cross-mod release gate (required, not optional late polish)

Compatibility inventory is added to **Stage 0 research**, read-only conflict diagnostics to **Stage 2**, interaction checks to **Stages 3–6** and the full matrix to **Stage 7**. No stage can silently defer a critical theme or tray regression to Stage 7.

Supported coexistence targets: Taskbar Styler (including SimplyTransparent and user custom running pips), Start Menu Styler, Notification Center Styler, Taskbar tray system icon tweaks and height/icon-size customizations. Animation references: Ph0en1x-dev original Dock Animation standalone baseline versus integrated dock animation. Two animation engines running on the same task buttons are a known likely conflict and must be detected / failed closed unless safe coordination is actually proven.

Per-stage acceptance: unchanged third-party mod configuration and appearance except user-selected overrides; valid native popup anchors; tray, clock, Start/search, notifications and Aero Peek operational; rollback does not reset any unrelated user's theme preferences. [Matrix](COMPATIBILITY.md).

## Stage 1a — external watchdog prototype (in development)

A prototype in `watchdog/` provides:
- separate-process 120-second deadline, persistent mock trial journal, file locks, before-apply snapshot and watchdog-ready acknowledgement;
- comparison-based mock rollback and manual recovery after a simulated watchdog kill;
- mock integration tests on both Windows and Linux.

**Not production-ready:** there is no Explorer/Windhawk IPC, native taskbar adapter, Windows-ready app packaging, shell-mod disabling, watchdog auto-respawn/startup recovery, or independently verified machine state restoration. The watchdog process can be killed; without a separate recovery process or the mod-side fail-closed integration, a modified mock can remain altered. This is why Stage 1's full gate remains **OPEN**, and Stages 3+ must remain blocked.

Review [watchdog/README.md](../watchdog/README.md) and the [Stage 1 safety specification](SAFETY.md).

## Stage 1b — mod-control adapter work (not yet shell-integrated)

An exact-mod-ID-only, dry-run-by-default Windhawk CLI adapter is present for Windhawk **2.x** (which has the official `windhawk-cli.exe`). Fake-runner tests validate version, command and error policy on Windows/Linux; no real Windhawk invocation or shell mutation has been performed. There is no automatic disable on watchdog expiry yet and no supported Windhawk 1.x independent-disable mechanism. Detailed acceptance requirements: [WINDHAWK_INTEGRATION.md](WINDHAWK_INTEGRATION.md).

The Stage 1 safety gate is still **not passed**. Development cannot proceed to live taskbar modifications until mod-side lease enforcement, startup recovery, durable real-property rollback, and failure injection are completed.

## Stage 1b diagnostic lease bridge status

- [x] Watchdog emits bounded versioned lease observation (mock trial only).
- [x] C++17 parser and latched stale/expired/mismatch rejection, tested on Windows and Linux.
- [x] Read-only Windhawk diagnostic skeleton polls for lease health with optional configured mock path/trial ID; always stops observation by 120s.
- [x] Single-source bundling script with static no-mutation checks.
- [ ] Trustworthy authenticated watchdog ⇄ Explorer control channel.
- [ ] Durable Windows system-property inverse journal, native runtime and recovery after hangs/restarts/reboots.
- [ ] Tested automatic Windhawk disable on user's installed version; no auto install or enable.
- [ ] Real taskbar change safety/reversibility demonstration.

**These completed boxes are for a non-mutating diagnostic, not completion of the Stage 1 safety gate.**

## Stage 1c — redundant mock failure recovery

- [x] Independent secondary supervisor starts and positively acknowledges before the mock change.
- [x] Two-way heartbeat monitor and compare-before-restore when either watchdog or supervisor stops updating the journal.
- [x] Kill-either-process tests added to Windows/Linux CI.
- [ ] Startup recovery after reboot or *both* processes failing.
- [ ] Authenticated IPC (current journal and command-line token are test-only).
- [ ] Native Windows property rollback and Windhawk per-mod disable confirmed in the installed version.

**Stage 1 continues to block all live taskbar modifications.**

## Stage 1d mock capability hardening

- [x] Pipe-delivered random per-trial credentials; no raw token in mock subprocess command lines.
- [x] Raw token excluded from on-disk mock journal; SHA-256 verifier with constant-time comparison.
- [x] Version 2 journal, older unresolved journals block new trials.
- [x] Local credential contract tests; cross-process integration checked by Windows/Linux CI.
- [ ] Authenticated Windows Explorer ↔ watchdog IPC with verified process/user identity and replay protection.
- [ ] Boot-time recovery, supported Windhawk auto-disable, real taskbar property restoration.

Stage 1 is **NOT** complete; live taskbar modifications remain blocked.

## Stage 1e — Windows-native authenticated IPC diagnostic

- [x] Portable strict challenge/response wire contract, locally compiled on Linux using C++17.
- [ ] Windows-native named-pipe probe: same-user SID, OS-reported client PID/session, local-only pipe ACL, mutual HMAC-SHA256 and random nonce/replay rejection. Implemented in test-only source; **GitHub Windows CI validation pending**.
- [ ] Bounded overlapped pipe I/O, verified trusted process identity, secure secret bootstrap, real mod/watchdog handshake.
- [ ] Boot-time recovery, true Windows state restoration and safe automatic Windhawk disabling.

No native hooks or taskbar-changing operations are present in the diagnostic. **Stage 1 safety gate is not passed.** See [native IPC README](../native/README.md).

## Stage 1f — bounded native IPC diagnostics

- [x] Standalone Windows named pipe uses a single bounded deadline and overlapped connect/read/write; timeouts cancel pending I/O and drain completions before releasing buffers.
- [x] Client independently verifies server PID, user token SID, session and path of the **test executable**; the server still checks client PID/session/SID.
- [x] Tests cover intentionally silent server, no client and wrong expected server PID.
- [ ] Integrate authenticated Windows IPC into the actual read-only Windhawk diagnostic with safe per-session secret provisioning. Current standalone probe is **not** this integration.
- [ ] Windows watchdog service/startup recovery, source-version-aware mod disable and real per-property Windows rollback.

**No live taskbar mutation is permitted.** See [native/README.md](../native/README.md).

## Stage 1g — Read-only Win32 IPC diagnostic integrated into mod source

The existing Windhawk *diagnostic source* now contains an optional Windows named-pipe observer (disabled by default). If explicitly configured with a temporary test pipe suffix, expected server PID and exact test server executable path, it attempts a single bounded, read-only IPC observation on its diagnostic worker thread. Missing settings, unexpected Windows process identity, invalid reply, and timeout must fail closed by ending observation. The default still only reads geometry.

A native Windows test host uses exactly the same IPC header as the mod; tests exercise successful peer observation, wrong PID/path, altered response, absent server and malformed suffix. The one-file bundler now embeds both the existing lease reader and the new bounded IPC code, and a CI-only Windhawk API declaration shim checks C++ syntax on Windows. **The shim is NOT proof it compiles or loads in the real Windhawk environment.**

This is **not production authenticated IPC**: no protected credential or challenge/response exists inside the Windhawk mod, the ACK is static, the executable path is not signature verification, and there is no trust established in the watchdog binary. Crucially, the diagnostic results **cannot authorize any taskbar modification**. Packaging, deployment, boot recovery, exact property restoration and Windhawk version-specific disabling are still blocking.
