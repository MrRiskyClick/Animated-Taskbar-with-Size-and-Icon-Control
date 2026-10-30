# Read-only Windhawk test prerequisites. Does NOT install or enable a mod.
# Does NOT write any Windows setting, taskbar, registry or Windhawk state.
# Use AFTER manually creating the exact local Windhawk developer mod in UI.
[CmdletBinding()]
param([switch]$CIOnlyFixture)

$ErrorActionPreference = 'Stop'
$lib=Join-Path $PSScriptRoot 'windhawk-17-own-mod-disarm.ps1'
if(-not (Test-Path -LiteralPath $lib)){throw "Missing prerequisite library: $lib"}
. $lib

if(-not $CIOnlyFixture){
    if(-not [Environment]::Is64BitOperatingSystem){
        throw "Native trial requires x64 Windows"
    }
    if(-not [Environment]::Is64BitProcess){
        throw "Use 64-bit PowerShell for the installed-mode test"
    }
    if(-not (Get-Process -Name explorer -ErrorAction SilentlyContinue)){
        throw "Explorer is not running: do not attempt the trial"
    }
}
# Only query the exact local-mod Disabled DWORD without requesting WRITE
# permission. Standard Windhawk installs protect HKLM from normal users.
Assert-NoAtbCompetingNativeDock -CIOnlyFixture:$CIOnlyFixture
$state=Assert-AtbOwnModReadable -CIOnlyFixture:$CIOnlyFixture
$mode=if($CIOnlyFixture){'CI registry fixture'}else{'installed Windhawk 1.7 local mod'}
Write-Host "[ATB] PASS: $mode exact Disabled setting found and readable (currently $state)"
if($state -eq 0) {
    throw 'This trial mod is enabled. Disable it in Windhawk before arming the watcher.'
}
Write-Host "[ATB] Mod is disabled, and no permission elevation is required."
Write-Host "[ATB] This check has NOT changed any registry, mod, or Windows state."
