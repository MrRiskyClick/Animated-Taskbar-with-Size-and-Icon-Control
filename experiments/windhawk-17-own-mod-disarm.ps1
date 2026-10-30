# Windhawk 1.7.x installed-mode OWN-MOD-ONLY safe disarm.
# Does not enumerate, update, or delete any other Windhawk mod or settings.
# This file contains functions; dot-sourcing has no side effects.
#
# Windhawk 1.7.3 source: src/windhawk/engine/storage_manager.cpp reads
# RegistryKey from engine.ini and GetModConfig(modName) under its Mods key;
# src/vscode-windhawk/src/utils/modConfigUtils.ts uses Disabled REG_DWORD.
# This module supports explicitly verified standard HKLM installation only.
# Portable and custom-base installations fail closed.
$script:AtbModKey = 'SOFTWARE\Windhawk\Engine\Mods\local@atb-animated-taskbar-trial'
function Open-AtbOwnModKey {
    param([switch]$Writable, [switch]$CIOnlyFixture)
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
        $target = $root.OpenSubKey($keyName,[bool]$Writable)
        if($null -eq $target) {
            throw "Own Windhawk mod registry key unavailable or not writable: $keyName"
        }
        return $target
    } finally { $root.Dispose() }
}
function Assert-AtbOwnModWritable {
    param([switch]$CIOnlyFixture)
    $key = Open-AtbOwnModKey -Writable -CIOnlyFixture:$CIOnlyFixture
    try {
        $kind = $key.GetValueKind('Disabled')
        $value = $key.GetValue('Disabled', $null,
                   [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        if($kind -ne [Microsoft.Win32.RegistryValueKind]::DWord -or
           $null -eq $value -or
           ($value -ne 0 -and $value -ne 1)) {
            throw 'Own local mod Disabled state is not REG_DWORD 0/1: refusing trial'
        }
        return [int]$value
    } finally { $key.Dispose() }
}
function Disable-OnlyAtbLocalMod {
    param([switch]$CIOnlyFixture)
    $key = Open-AtbOwnModKey -Writable -CIOnlyFixture:$CIOnlyFixture
    try {
        if($key.GetValueKind('Disabled') -ne
           [Microsoft.Win32.RegistryValueKind]::DWord) {
            throw 'Own mod Disabled registry value unexpectedly changed type'
        }
        $disabled = $key.GetValue('Disabled', $null)
        if($null -eq $disabled -or
           ($disabled -ne 0 -and $disabled -ne 1)) {
            throw 'Own mod Disabled value not 0 or 1; refusing registry mutation'
        }
        if($disabled -eq 0) {
            # EXACT original local mod key and EXACT Disabled value only.
            $key.SetValue('Disabled',1,
                [Microsoft.Win32.RegistryValueKind]::DWord)
            $key.Flush()
        }
        if($key.GetValue('Disabled', $null) -ne 1) {
            throw 'Own mod could not be verified disabled'
        }
        return $true
    } finally { $key.Dispose() }
}
