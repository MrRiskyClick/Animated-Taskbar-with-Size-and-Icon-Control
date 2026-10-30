# Windhawk 1.7.x installed-mode OWN-MOD READ-ONLY inspection.
# No registry writes or elevation required. Dot-sourcing has no side effects.
# The ATB2 lease's exact Explorer PID, live watcher, heartbeat and deadline
# stop reapplication on a new Explorer after manual restart/reboot.
#
# Windhawk 1.7.3 source: src/windhawk/engine/storage_manager.cpp reads
# RegistryKey from engine.ini and GetModConfig(modName) under its Mods key;
# src/vscode-windhawk/src/utils/modConfigUtils.ts uses Disabled REG_DWORD.
# This module supports explicitly verified standard HKLM installation only.
# Portable and custom-base installations fail closed.
$script:AtbModKey = 'SOFTWARE\Windhawk\Engine\Mods\local@atb-animated-taskbar-trial'
function Open-AtbOwnModKey {
    param([switch]$CIOnlyFixture)
    $keyName = if($CIOnlyFixture) {
        'Software\AnimatedTaskbarCI\Tests\local@atb-animated-taskbar-trial'
    } else {
        $script:AtbModKey
    }
    $hive = if($CIOnlyFixture) {
        [Microsoft.Win32.RegistryHive]::CurrentUser
    } else {
        [Microsoft.Win32.RegistryHive]::LocalMachine
    }
    $root = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        $hive, [Microsoft.Win32.RegistryView]::Registry64)
    try {
        $target = $root.OpenSubKey($keyName,$false)
        if($null -eq $target) {
            throw "Own Windhawk mod registry key unavailable for read-only inspection: $keyName"
        }
        return $target
    } finally { $root.Dispose() }
}
function Assert-AtbOwnModReadable {
    param([switch]$CIOnlyFixture)
    $key = Open-AtbOwnModKey -CIOnlyFixture:$CIOnlyFixture
    try {
        $kind = $key.GetValueKind('Disabled')
        $value = $key.GetValue('Disabled', $null,
                   [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        if($kind -ne [Microsoft.Win32.RegistryValueKind]::DWord -or
           $null -eq $value -or
           ($value -ne 0 -and $value -ne 1)) {
            throw 'Own local mod Disabled is not readable REG_DWORD 0/1: refusing trial'
        }
        return [int]$value
    } finally { $key.Dispose() }
}

# Read-only compatibility gate. Two mods applying independent pointer hooks
# and XAML transforms to the SAME task buttons cannot be treated as safe.
# Never change the original dock animation mod's state automatically.
function Assert-NoAtbCompetingNativeDock {
    param([switch]$CIOnlyFixture)
    $baseKey = if($CIOnlyFixture) {
        'Software\AnimatedTaskbarCI\Tests'
    } else {
        'SOFTWARE\Windhawk\Engine\Mods'
    }
    $hive = if($CIOnlyFixture) {
        [Microsoft.Win32.RegistryHive]::CurrentUser
    } else {
        [Microsoft.Win32.RegistryHive]::LocalMachine
    }
    $root = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        $hive, [Microsoft.Win32.RegistryView]::Registry64)
    try {
        foreach($id in @('taskbar-dock-animation','local@taskbar-dock-animation')) {
            $key=$root.OpenSubKey("$baseKey\$id",$false)
            if($null -eq $key){continue}
            try {
                if($key.GetValueKind('Disabled') -ne
                    [Microsoft.Win32.RegistryValueKind]::DWord) {
                    throw "Cannot verify whether competing mod $id is disabled; refusing trial"
                }
                $disabled=$key.GetValue('Disabled',$null)
                if($null -eq $disabled -or $disabled -ne 1) {
                    throw "Competing native animation mod $id is not verified disabled. This trial will not change it automatically."
                }
            } finally { $key.Dispose() }
        }
    } finally { $root.Dispose() }
}
