# Developer-only Animated Taskbar lease watcher for Windows PowerShell 5.1+
# No administrator rights, registry writes, taskbar edits, process termination,
# service installation, or edits to other Windhawk mods.
# All active trial effects require this independent process to remain responsive.
param([switch]$SelfTest)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AtbWin32 {
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool MoveFileEx(string oldPath, string newPath, uint flags);
}
'@
$durationMs = if ($SelfTest) { 2500 } else { 120000 }
$base = if ($SelfTest) {
    Join-Path $env:TEMP ('atb-dev-selftest-' + $PID)
} else {
    Join-Path $env:LOCALAPPDATA 'AnimatedTaskbarTrial'
}
[System.IO.Directory]::CreateDirectory($base) | Out-Null
$lease = Join-Path $base 'lease.v1'
$mutexPath = Join-Path $base 'watcher.lock'
$lock = $null
try {
    $lock = [System.IO.File]::Open($mutexPath,
        [System.IO.FileMode]::OpenOrCreate,
        [System.IO.FileAccess]::ReadWrite,
        [System.IO.FileShare]::None)
    [uint32] $explorerPid = 0
    if ($SelfTest) {
        # Fully isolated CI test. Never discover or interact with real Explorer.
        $explorerPid = [uint32]$PID
    } else {
        $hwnd = [AtbWin32]::FindWindow('Shell_TrayWnd', $null)
        if ($hwnd -eq [IntPtr]::Zero) { throw "Windows primary taskbar not found." }
        [void][AtbWin32]::GetWindowThreadProcessId($hwnd, [ref] $explorerPid)
        if ($explorerPid -eq 0) { throw "Could not read the taskbar Explorer PID." }
    }

    $startUtc = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $deadlineUtc = $startUtc + $durationMs
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $revoked = $false
    function Publish-Lease([string]$content) {
        $tmp = Join-Path $base ("lease-" + [guid]::NewGuid().ToString('N') + '.tmp')
        try {
            [System.IO.File]::WriteAllText($tmp, $content,
                [System.Text.Encoding]::ASCII)
            # MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
            if (-not [AtbWin32]::MoveFileEx($tmp, $lease, 9)) {
                throw "Atomic lease update failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
            }
        } finally {
            if ([System.IO.File]::Exists($tmp)) {
                [System.IO.File]::Delete($tmp)
            }
        }
    }

    Write-Host ("[ATB] Trial watcher PID {0}, Explorer PID {1}" -f $PID, $explorerPid)
    Write-Host ("[ATB] Lease expires after {0} milliseconds." -f $durationMs)
    if (-not $SelfTest) { Write-Host "[ATB] Enable the local Windhawk trial mod." }
    Write-Host "[ATB] This is NOT a production-grade recovery agent."

    try {
        while ($clock.ElapsedMilliseconds -lt $durationMs) {
            if (-not $SelfTest) {
                $currentHwnd = [AtbWin32]::FindWindow('Shell_TrayWnd', $null)
                [uint32]$currentExplorerPid = 0
                if ($currentHwnd -eq [IntPtr]::Zero) { break }
                [void][AtbWin32]::GetWindowThreadProcessId($currentHwnd, [ref] $currentExplorerPid)
                if ($currentExplorerPid -ne $explorerPid) {
                    Write-Host "[ATB] Explorer changed. Revoking the trial."
                    break
                }
            }
            $nowUtc = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
            if ($nowUtc -ge $deadlineUtc) { break }
            Publish-Lease ("ATB1 {0} {1} {2} {3}`n" -f
                $explorerPid, $deadlineUtc, $nowUtc, $PID)
            Start-Sleep -Milliseconds 200
        }
    } finally {
        Publish-Lease "REVOKED`n"
        $revoked = $true
        Write-Host "[ATB] Lease revoked. Native module must stop visual transforms."
    }
} finally {
    if ($null -ne $lock) { $lock.Dispose() }
}
