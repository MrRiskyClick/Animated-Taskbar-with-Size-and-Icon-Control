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
# A successful return from this exact open attempt verifies permission to
# set only the single local-mod Disabled DWORD. No SetValue is invoked.
$state=Assert-AtbOwnModWritable -CIOnlyFixture:$CIOnlyFixture
$mode=if($CIOnlyFixture){'CI registry fixture'}else{'installed Windhawk 1.7 local mod'}
Write-Host "[ATB] PASS: $mode exact Disabled setting found and writable (currently $state)"
if($state -eq 0) {
    Write-Warning "This mod is currently ENABLED. Disable it in Windhawk before attempting a new controlled test."
} else {
    Write-Host "[ATB] Mod is disabled as expected for a supervised trial."
}
Write-Host "[ATB] This check has NOT changed any registry, mod, or Windows state."
