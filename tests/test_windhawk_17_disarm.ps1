# GitHub Windows CI fixture ONLY. No writes to actual Windhawk.
# This fixture sets up disposable HKCU test keys, after which all imported
# inspection functions and the preflight are READ ONLY.
$ErrorActionPreference='Stop'
$root='HKCU:\Software\AnimatedTaskbarCI\Tests'
$ours=Join-Path $root 'local@atb-animated-taskbar-trial'
$foreign=Join-Path $root 'local@unrelated-mod'
$competing=Join-Path $root 'taskbar-dock-animation'
$module=Join-Path $PSScriptRoot '..\experiments\windhawk-17-own-mod-disarm.ps1'
$preflight=Join-Path $PSScriptRoot '..\experiments\check-local-trial-prerequisites.ps1'
$watcher=Join-Path $PSScriptRoot '..\experiments\run-native-overhang-trial.ps1'
try {
    New-Item -Path $ours,$foreign,$competing -Force | Out-Null
    New-ItemProperty -Path $ours -Name Disabled -Value 1 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $foreign -Name Disabled -Value 0 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $competing -Name Disabled -Value 1 -PropertyType DWord -Force | Out-Null
    . $module
    if((Assert-AtbOwnModReadable -CIOnlyFixture) -ne 1) {
        throw 'Own mod must initially be disabled for a supervised trial'
    }
    Assert-NoAtbCompetingNativeDock -CIOnlyFixture
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $preflight -CIOnlyFixture
    if($LASTEXITCODE -ne 0){throw 'Normal read-only fixture preflight failed'}
    if((Get-ItemProperty -LiteralPath $ours).Disabled -ne 1 -or
       (Get-ItemProperty -LiteralPath $foreign).Disabled -ne 0 -or
       (Get-ItemProperty -LiteralPath $competing).Disabled -ne 1) {
        throw 'Preflight modified a registry value'
    }
    Set-ItemProperty -LiteralPath $competing -Name Disabled -Value 0
    $rejected=$false
    try {Assert-NoAtbCompetingNativeDock -CIOnlyFixture} catch {$rejected=$true}
    if(-not $rejected){throw 'Competing mod should be rejected without mutation'}
    if((Get-ItemProperty -LiteralPath $competing).Disabled -ne 0) {
        throw 'Competing mod registry state was modified'
    }
    Set-ItemProperty -LiteralPath $competing -Name Disabled -Value 1
    Set-ItemProperty -LiteralPath $ours -Name Disabled -Value 2
    $rejected=$false
    try {Assert-AtbOwnModReadable -CIOnlyFixture | Out-Null} catch {$rejected=$true}
    if(-not $rejected){throw 'Malformed own-mod state should be rejected'}
    Set-ItemProperty -LiteralPath $ours -Name Disabled -Value 1
    # No production registry mutation API is permitted in the watchdog.
    $source = [IO.File]::ReadAllText($module) + [IO.File]::ReadAllText($watcher) +
              [IO.File]::ReadAllText($preflight)
    foreach($notAllowed in @('SetValue(','Remove-ItemProperty','Set-ItemProperty',
                             'RegSetValue','reg.exe add','Disable-OnlyAtbLocalMod',
                             'OpenSubKey($keyName,[bool]$Writable)')) {
        if($source.Contains($notAllowed)){throw "Elevated write action remains: $notAllowed"}
    }
    Write-Host 'PASS: own-mod registry checks are read-only, no elevation required'
    Write-Host 'PASS: competing mod stays untouched; malformed state fails closed'
} finally {
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
