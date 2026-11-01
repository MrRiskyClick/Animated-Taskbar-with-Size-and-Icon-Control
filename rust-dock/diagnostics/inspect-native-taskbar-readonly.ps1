# Read-only Explorer taskbar inventory for matching icon order and icon resources.
# Run with Windows PowerShell 5.1 while your *native* taskbar icons are visible.
# No registry writes, pin manipulation, Explorer restart, modification or elevation.
[CmdletBinding()]
param([string]$OutPath = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'ATB-taskbar-inventory.json'))
$ErrorActionPreference='Stop'
$report=[ordered]@{
  time=(Get-Date).ToString('o')
  windowsVersion=[Environment]::OSVersion.VersionString
  taskbarWindowPresent=$false
  taskbarButtons=@()
  pinnedShortcuts=@()
  warnings=@()
}
try {
  if (-not ('ATBReadOnlyTaskbarProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ATBReadOnlyTaskbarProbe {
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr FindWindowW(string className, string caption);
}
'@
  }
  $taskbar=[ATBReadOnlyTaskbarProbe]::FindWindowW('Shell_TrayWnd',$null)
  $report.taskbarWindowPresent=($taskbar -ne [IntPtr]::Zero)
  if ($report.taskbarWindowPresent) {
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    $root=[System.Windows.Automation.AutomationElement]::FromHandle($taskbar)
    $nodes=$root.FindAll(
      [System.Windows.Automation.TreeScope]::Descendants,
      [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($node in $nodes) {
      try {
        $c=$node.Current
        $kind=$c.ControlType.ProgrammaticName
        if ($kind -notin @('ControlType.Button','ControlType.ListItem','ControlType.Image','ControlType.Custom')) {continue}
        $r=$c.BoundingRectangle
        if ($r.Width -lt 4 -or $r.Height -lt 4) {continue}
        $report.taskbarButtons+= [ordered]@{
          controlType=$kind
          name=$c.Name
          automationId=$c.AutomationId
          className=$c.ClassName
          frameworkId=$c.FrameworkId
          offscreen=$c.IsOffscreen
          x=[int]$r.X
          y=[int]$r.Y
          width=[int]$r.Width
          height=[int]$r.Height
        }
      } catch {
        # Accessibility tree changes while apps open and close.
      }
    }
    $report.taskbarButtons=@($report.taskbarButtons | Sort-Object y,x)
  } else {
    $report.warnings+= 'Shell_TrayWnd is not present. Inventory needs an Explorer desktop session.'
  }
} catch {
  $report.warnings+= ('Read-only UI Automation unavailable: '+$_.Exception.Message)
}
try {
  $pins=Join-Path $env:APPDATA 'Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar'
  if (Test-Path -LiteralPath $pins) {
    $shell=New-Object -ComObject WScript.Shell
    foreach ($lnk in @(Get-ChildItem -LiteralPath $pins -File -Filter '*.lnk')) {
      try {
        $link=$shell.CreateShortcut($lnk.FullName)
        $report.pinnedShortcuts+= [ordered]@{
          name=$lnk.BaseName
          shortcut=$lnk.FullName
          target=$link.TargetPath
          arguments=$link.Arguments
          iconLocation=$link.IconLocation
        }
      } catch {
        $report.warnings+= ('Cannot read shortcut '+$lnk.Name+': '+$_.Exception.Message)
      }
    }
  } else {
    $report.warnings+= 'Pinned shortcut folder was not found; Windows 11 may store some pins elsewhere.'
  }
} catch {
  $report.warnings+= ('Read-only shortcut enumeration unavailable: '+$_.Exception.Message)
}
$parent=Split-Path -Parent $OutPath
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
  throw 'Output directory does not exist. Choose a file inside an existing folder.'
}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutPath -Encoding UTF8
Write-Host "Read-only taskbar inventory saved: $OutPath"
Write-Host ("Visible UI Automation elements: "+@($report.taskbarButtons).Count)
Write-Host ("Pinned shortcut metadata entries: "+@($report.pinnedShortcuts).Count)
if (@($report.warnings).Count) { $report.warnings | ForEach-Object {Write-Warning $_} }
Write-Host 'Review report before sharing: it contains app names, labels, shortcut paths and icon locations.'
