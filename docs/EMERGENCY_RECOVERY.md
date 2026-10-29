# Emergency recovery runbook (Windows 11 / Windhawk)

## Protection layers

1. **Primary:** before any active taskbar modifications, the project must implement and test the independent 120-second watchdog and an inverse-operation journal. That protection is **not implemented yet**.
2. **Secondary:** a manual exit path should be accessible even with a malfunctioning taskbar (Task Manager, keyboard shortcuts, safe mode, and verified Windhawk disable procedure).
3. **Tertiary:** Windows **System Restore** or a verified system-image backup. These help with severe damage but are neither deterministic rollback nor a substitute for layer 1.

## Windows System Restore point

Creating a named **manual System Restore point before testing** is recommended. The snapshot is of Windows system components/settings and installed software state; it is **not** a complete copy of a system drive or user's documents. A restore can remove installed apps or revert settings changed after the point, and can fail or be unavailable.

System Restore differs from Microsoft's newer **Point-in-time restore** feature (which can restore personal files on supported systems). Never conflate their semantics.

Before testing:
- Confirm the desired restore point appears in System Restore's UI.
- Prefer a tested independent backup of important files/system image for experiments involving unstable Explorer hooks.
- Confirm Windows Recovery Environment (WinRE) is usable.
- If drive encryption/BitLocker is enabled, store recovery key outside the PC securely.
- Do not store user-specific restore point labels, timestamps, screenshots or recovery keys in this **public repository**.

## If Explorer/taskbar is broken but Windows is running

- Try `Ctrl+Shift+Esc` to open Task Manager.
- In Task Manager, use **Run new task** to open the verified Windhawk interface or recovery control; if appropriate, disable the experimental mod first. Do not assume a command-line API to disable Windhawk exists until verified.
- After preventing reapplication, restart Windows Explorer or reboot if safe.
- Verify taskbar geometry, tray, pinned icons and work area are back before rearming any experiment.

## If Windows repeatedly fails to boot

- Enter WinRE (Windows may open it automatically after repeated failed starts; alternatively use Windows recovery media).
- Navigate **Troubleshoot → Advanced options → System Restore**, choose the pre-test point and follow prompts.
- If asked, provide the BitLocker recovery key.
- After restoration, verify that affected programs and Windhawk settings/versions have reverted as expected.
- If restore is missing or fails, other options include Startup Repair, Safe Mode, uninstalling a problematic update, or recovery from a full system-image backup.

A BSOD does not prove the restore point will be accessible or that restoration will succeed. Preserve Windows event logs/crash dump and issue details once stable.

## Microsoft's instructions

- System Restore: https://support.microsoft.com/en-us/windows/experience/backup-recovery/system-restore
- Windows Recovery Environment: https://support.microsoft.com/en-us/windows/experience/backup-recovery/windows-recovery-environment
- Windows recovery options: https://support.microsoft.com/en-us/windows/experience/backup-recovery/recovery-options-in-windows
