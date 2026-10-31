# ATB Native Rust Dock — v0.1 developer prototype

**Independent application; NOT a Windhawk DLL.** Windows 11 only. No installer,
service, WebView, JavaScript runtime or separate Windows taskbar replacement.
This is a standalone native Rust/Win32 executable, designed to coexist with
the separate `ATB Hide Taskbar Buttons` Windhawk mod and the user's
`SimplyTransparent` Taskbar Styler theme.

## v0.1 goal

- Display icons for currently open, user-visible windows, grouped by executable.
- Spring-driven icon magnification that pushes neighbors apart instead of overlapping.
- Native DWM *live* thumbnail previews in an app-owned top-level window.
- Show a single thumbnail for one running window, or a grid for grouped windows.
- Configurable hover and dismissal delays; moving into the preview panel
  keeps it open. Click a thumbnail to activate its source window; X closes
  that specific window by sending `WM_CLOSE` (not killing the process).
- All preview registrations are released on close/rebuild/exit.
- The dock touches **no** Explorer XAML, window region, work-area reservation,
  `SHAppBarMessage(ABM_SETPOS)`, registry or Windhawk settings.

This is an early native interaction prototype, **not** the final full dock.
Pinned applications without open windows, Start/Task View replacements,
custom icons/configuration UI, context menus, progress, multiple monitors,
per-app grouping rules, fullscreen detection, and reliable DPI adaptation
are tracked as follow-up work. Do NOT hide the native taskbar buttons until
the Rust dock has been validated and can show your actual windows reliably.

## Developer commands (do not run until the user requests a build to test)

```powershell
cd rust-dock
cargo test
cargo build --release
.\target\release\atb-rust-dock.exe --hover-delay-ms=450 --dismiss-delay-ms=250
```

No Explorer restart needed. Closing this EXE destroys its own dock and
preview windows; Windows Explorer's native taskbar remains untouched.
The separate Windhawk hiding mod may leave application buttons hidden
if it is still enabled, which can be manually toggled off.

## Technical design

The DWM registration destination is an owned `WS_POPUP` preview
window (not Explorer's Shell_TrayWnd). Each top-level source window
gets its own `DwmRegisterThumbnail` and `DwmUpdateThumbnailProperties`
rectangle. A small central Win32 message loop drives mouse hover/dismissal
timers and only animates the icon row while the spring is unsettled.

Read-only window enumeration uses Win32; activation uses `SetForegroundWindow`
and `ShowWindow(SW_RESTORE)`. DWM thumbnails aren't screenshots and
update continuously while the previews are open.

Reference: https://learn.microsoft.com/en-us/windows/win32/dwm/thumbnail-ovw
