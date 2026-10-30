# GitHub-hosted Windows CI test ONLY. Modifies only a disposable HKCU fixture,
# never any Windhawk path or the user's PC.
$ErrorActionPreference='Stop'
$root='HKCU:\Software\AnimatedTaskbarCI\Tests'
$ours=Join-Path $root 'local@atb-animated-taskbar-trial'
$foreign=Join-Path $root 'local@unrelated-mod'
$module=Join-Path $PSScriptRoot '..\experiments\windhawk-17-own-mod-disarm.ps1'
try {
 New-Item -Path $ours -Force | Out-Null
 New-Item -Path $foreign -Force | Out-Null
 New-ItemProperty -Path $ours -Name Disabled -Value 0 -PropertyType DWord -Force | Out-Null
 New-ItemProperty -Path $foreign -Name Disabled -Value 0 -PropertyType DWord -Force | Out-Null
 . $module
 if((Assert-AtbOwnModWritable -CIOnlyFixture) -ne 0){
  throw 'The own mod should initially be enabled'
 }
 if(-not (Disable-OnlyAtbLocalMod -CIOnlyFixture)){
  throw 'Own-mod registry disarm returned false'
 }
 if((Get-ItemProperty -LiteralPath $ours -Name Disabled).Disabled -ne 1){
  throw 'Own local mod was not disabled'
 }
 if((Get-ItemProperty -LiteralPath $foreign -Name Disabled).Disabled -ne 0){
  throw 'SECURITY FAILURE: unrelated local mod was touched'
 }
 # Simulate a fresh PowerShell session after a manual Explorer restart/reboot:
 # the only persistent state is our mod's own Disabled=1. This uses a
 # disposable HKCU fixture, not any real Windhawk registry key.
 $verify = @'
$own = 'HKCU:\Software\AnimatedTaskbarCI\Tests\local@atb-animated-taskbar-trial'
$other = 'HKCU:\Software\AnimatedTaskbarCI\Tests\local@unrelated-mod'
if ((Get-ItemProperty -LiteralPath $own -Name Disabled).Disabled -ne 1) { exit 91 }
if ((Get-ItemProperty -LiteralPath $other -Name Disabled).Disabled -ne 0) { exit 92 }
exit 0
'@
 & pwsh -NoProfile -Command $verify
 if ($LASTEXITCODE -ne 0) {
   throw "Fresh process did not observe the own-mod-only persistent disable ($LASTEXITCODE)"
 }
 Write-Host 'PASS: fresh process sees only our mod disabled (reboot/restart persistence fixture)'
 $preflight = Join-Path $PSScriptRoot '..\experiments\check-local-trial-prerequisites.ps1'
 & pwsh -NoProfile -File $preflight -CIOnlyFixture
 if ($LASTEXITCODE -ne 0) { throw "Read-only preflight failed in CI fixture" }
 if((Get-ItemProperty -LiteralPath $ours -Name Disabled).Disabled -ne 1 -or
    (Get-ItemProperty -LiteralPath $foreign -Name Disabled).Disabled -ne 0){
  throw "Preflight modified installed-mode fixture registry state"
 }
 if(-not (Disable-OnlyAtbLocalMod -CIOnlyFixture)){
  throw 'Repeated own-mod disable should be idempotent'
 }
 Set-ItemProperty -LiteralPath $ours -Name Disabled -Value 2
 $refused=$false
 try { Disable-OnlyAtbLocalMod -CIOnlyFixture | Out-Null }
 catch { $refused=$true }
 if(-not $refused){throw 'Malformed own-mod registry status was incorrectly accepted'}
 Write-Host 'PASS: own local mod disabled, unrelated mod preserved, malformed status rejected'
} finally {
 # Clean up our disposable CI fixture only. Never enumerate Windhawk keys.
 if(Test-Path -LiteralPath $root){
   Remove-Item -LiteralPath $root -Recurse -Force
 }
}
