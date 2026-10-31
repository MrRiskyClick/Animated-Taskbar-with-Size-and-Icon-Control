# GitHub-developed experimental independent taskbar-geometry lease/watchdog.
# PowerShell 5.1+. No installer, startup task, shell restart, auto-hide
# or registry writes at all. Run as NORMAL USER, never elevated.
# ATB3 also snapshots any pre-existing native taskbar HWND region for exact
# compare-before-replacement and watchdog restoration.
# Reapplication is prevented by a 120-second irrevocable lease bound to
# the exact current Explorer PID and an independent live watcher process.
# On expiry, the mod may still show enabled in Windhawk's UI, but remains
# inert until a fresh user-started and PID-matched lease.
# Requires explicit user execution for a future controlled trial.
param([switch]$SelfTest, [switch]$TestUnresolved)
$ErrorActionPreference='Stop'
# This trial never needs a registry write. The mod reads only its expiring
# lease and remains inert after expiration or manual Explorer restart.
$disarmModule=Join-Path $PSScriptRoot 'windhawk-17-own-mod-disarm.ps1'
if(-not (Test-Path -LiteralPath $disarmModule)){
 throw 'Missing own-mod read-only inspection module; refusing native geometry trial'
}
. $disarmModule
# PowerShell 5.1 keeps Add-Type classes for the lifetime of its host.
# Use a protocol-revision-specific name and reuse it on later invocations;
# ATB2's earlier class (AtbGeoWin32) must never supply ATB3's ABI.
if (-not ('AtbGeoWin32_ATB3_R1' -as [type])) {
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AtbGeoWin32_ATB3_R1 {
 [StructLayout(LayoutKind.Sequential)]
 public struct RECT { public int Left, Top, Right, Bottom; }
 [StructLayout(LayoutKind.Sequential)]
 public struct MONITORINFO { public uint cbSize; public RECT monitor; public RECT work; public uint flags; }
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll",SetLastError=true)]
 public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
 [DllImport("user32.dll",SetLastError=true)]
 [return:MarshalAs(UnmanagedType.Bool)]
 public static extern bool GetWindowRect(IntPtr hwnd,out RECT rect);
 [DllImport("user32.dll",SetLastError=true)]
 [return:MarshalAs(UnmanagedType.Bool)]
 public static extern bool SetWindowPos(IntPtr hwnd,IntPtr behind,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll",SetLastError=true)]
 public static extern IntPtr MonitorFromWindow(IntPtr hwnd,uint flags);
 [DllImport("user32.dll",SetLastError=true)]
 [return:MarshalAs(UnmanagedType.Bool)]
 public static extern bool GetMonitorInfo(IntPtr mon,ref MONITORINFO info);
 [DllImport("user32.dll",SetLastError=true)]
 public static extern uint GetDpiForWindow(IntPtr hwnd);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern IntPtr GetProp(IntPtr hwnd,string prop);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern IntPtr RemoveProp(IntPtr hwnd,string prop);
 [DllImport("user32.dll",SetLastError=true)]
 public static extern int SetWindowRgn(IntPtr hwnd,IntPtr region,bool redraw);
 [DllImport("gdi32.dll",SetLastError=true)]
 public static extern IntPtr CreateRectRgn(int left,int top,int right,int bottom);
 [DllImport("gdi32.dll",SetLastError=true)]
 public static extern bool DeleteObject(IntPtr region);
 [DllImport("gdi32.dll",SetLastError=true)]
 public static extern int CombineRgn(IntPtr dst,IntPtr src1,IntPtr src2,int mode);
 [DllImport("gdi32.dll",SetLastError=true)]
 public static extern uint GetRegionData(IntPtr region,uint length,byte[] buffer);
 [DllImport("user32.dll",SetLastError=true)]
 public static extern int GetWindowRgn(IntPtr hwnd,IntPtr region);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern IntPtr CreateWindowEx(uint style,string cls,
   string name,uint windowStyle,int x,int y,int width,int height,
   IntPtr parent,IntPtr menu,IntPtr instance,IntPtr data);
 [DllImport("user32.dll",SetLastError=true)]
 public static extern bool DestroyWindow(IntPtr hwnd);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern bool SetProp(IntPtr hwnd,string name,IntPtr value);
 public static ulong RegionHash(IntPtr region) {
   uint length=GetRegionData(region,0,null);
   if(length<32 || length>1048576)return 0;
   byte[] buffer=new byte[length];
   if(GetRegionData(region,length,buffer)!=length)return 0;
   ulong hash=14695981039346656037UL;
   unchecked {foreach(byte b in buffer){hash^=b;hash*=1099511628211UL;}}
   return hash==0?1:hash;
 }
 public static bool RegionFingerprintSelfTest() {
   // Completely separate temporary STATIC HWND, no Explorer or taskbar calls.
   var hwnd=CreateWindowEx(0,"STATIC","ATB owned region test",0x80000000,
                  10,10,500,100,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);
   if(hwnd==IntPtr.Zero)return false;
   try {
     var ours=CreateRectRgn(0,40,500,100);
     if(ours==IntPtr.Zero)return false;
     ulong hash=RegionHash(ours);
     if(hash==0){DeleteObject(ours);return false;}
     if(SetWindowRgn(hwnd,ours,false)==0){DeleteObject(ours);return false;}
     if(!SetProp(hwnd,"ATB_REGION_FINGERPRINT_V2",
                 new IntPtr(unchecked((long)hash))))return false;
     bool owned=RegionFingerprintMatches(hwnd);
     var foreign=CreateRectRgn(100,40,500,100);
     if(foreign==IntPtr.Zero)return false;
     if(SetWindowRgn(hwnd,foreign,false)==0){DeleteObject(foreign);return false;}
     bool rejected=!RegionFingerprintMatches(hwnd);
     RemoveProp(hwnd,"ATB_REGION_FINGERPRINT_V2");
     return owned && rejected;
   } finally {DestroyWindow(hwnd);}
 }
 // A non-empty native region can be preserved and restored exactly.
 // Only a disposable HWND owned by this short-lived CI/self-test process.
 public static bool RegionPreservationSelfTest() {
   var hwnd=CreateWindowEx(0,"STATIC","ATB baseline-preservation test",
      0x80000000,10,10,500,100,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);
   if(hwnd==IntPtr.Zero)return false;
   var snapshot=CreateRectRgn(0,0,0,0);
   try {
     var baseRgn=CreateRectRgn(17,12,487,91);
     if(baseRgn==IntPtr.Zero)return false;
     if(SetWindowRgn(hwnd,baseRgn,false)==0){
       DeleteObject(baseRgn);return false;
     }
     if(GetWindowRgn(hwnd,snapshot)==0)return false;
     var expected=RegionHash(snapshot);
     if(expected==0)return false;
     var expanded=CreateRectRgn(0,0,500,100);
     if(expanded==IntPtr.Zero)return false;
     if(SetWindowRgn(hwnd,expanded,false)==0){
       DeleteObject(expanded);return false;
     }
     var restored=CreateRectRgn(0,0,0,0);
     if(restored==IntPtr.Zero)return false;
     if(CombineRgn(restored,snapshot,IntPtr.Zero,5)==0){
       DeleteObject(restored);return false;
     }
     if(SetWindowRgn(hwnd,restored,false)==0){
       DeleteObject(restored);return false;
     }
     var current=CreateRectRgn(0,0,0,0);
     if(current==IntPtr.Zero)return false;
     try {
       return GetWindowRgn(hwnd,current)!=0 &&
              RegionHash(current)==expected;
     } finally {DeleteObject(current);}
   } finally {if(snapshot!=IntPtr.Zero)DeleteObject(snapshot);DestroyWindow(hwnd);}
 }
 public static bool RegionFingerprintMatches(IntPtr hwnd) {
  var property = GetProp(hwnd,"ATB_REGION_FINGERPRINT_V2");
  if(property==IntPtr.Zero)return false;
  IntPtr region=CreateRectRgn(0,0,0,0);
  if(region==IntPtr.Zero)return false;
  try {
   if(GetWindowRgn(hwnd,region)==0)return false; // ERROR, not owned
   ulong hash=RegionHash(region);
   return hash!=0 && hash==unchecked((ulong)property.ToInt64());
  } finally { DeleteObject(region); }
 }
 [DllImport("user32.dll",SetLastError=true)]
 public static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint msg,IntPtr wParam,IntPtr lParam,uint flags,uint timeout,out IntPtr result);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 [return:MarshalAs(UnmanagedType.Bool)]
 public static extern bool MoveFileEx(string from,string to,uint flags);
}
'@
}
$durationMs=if($SelfTest){2500}else{120000}
$root=if($SelfTest){
 Join-Path $env:TEMP ("atb-native-overhang-selftest-"+$PID)
}else{
 Join-Path $env:LOCALAPPDATA 'AnimatedTaskbarTrial'
}
if($SelfTest -and (-not [AtbGeoWin32_ATB3_R1]::RegionFingerprintSelfTest() -or
                    -not [AtbGeoWin32_ATB3_R1]::RegionPreservationSelfTest())){
 throw 'Disposable HWND ownership/baseline-region restoration self-test failed'
}
[IO.Directory]::CreateDirectory($root)|Out-Null
$lease=Join-Path $root 'lease.v1'
$lockPath=Join-Path $root 'watcher.lock'
$lock=$null
$trialHwnd=[IntPtr]::Zero
$baselineRegion=[IntPtr]::Zero  # Owned by this independent watcher process
$regionMode=0
[UInt64]$regionHash=0
$before=$null
$targetTop=0
$explorerPid=[uint32]$PID
$baselineWork=$null
function Write-Lease([string]$body){
 $tmp=Join-Path $root ("lease-"+[Guid]::NewGuid().ToString('N')+".tmp")
 try {
  [IO.File]::WriteAllText($tmp,$body,[Text.Encoding]::ASCII)
  # MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
  if(-not [AtbGeoWin32_ATB3_R1]::MoveFileEx($tmp,$lease,9)){
   throw "Atomic lease update failed. Win32: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
  }
 }finally{
  if([IO.File]::Exists($tmp)){[IO.File]::Delete($tmp)}
 }
}
function Same-Rect($a,$b){
 return ($a.Left -eq $b.Left -and $a.Top -eq $b.Top -and
         $a.Right -eq $b.Right -and $a.Bottom -eq $b.Bottom)
}
function Stop-NativeTrial(){
 if($SelfTest){return}
 if($trialHwnd -eq [IntPtr]::Zero -or $null -eq $before){return}
 $nowHwnd=[AtbGeoWin32_ATB3_R1]::FindWindow('Shell_TrayWnd',$null)
 [uint32]$nowPid=0
 if($nowHwnd -ne $trialHwnd){Write-Warning 'Taskbar HWND changed; no geometry write';return}
 [void][AtbGeoWin32_ATB3_R1]::GetWindowThreadProcessId($nowHwnd,[ref]$nowPid)
 if($nowPid -ne $explorerPid){Write-Warning 'Explorer PID changed; no geometry write';return}
 $rect=New-Object AtbGeoWin32+RECT
 if(-not [AtbGeoWin32_ATB3_R1]::GetWindowRect($trialHwnd,[ref]$rect)){return}
 $expected=New-Object AtbGeoWin32+RECT
 $expected.Left=$before.Left
 $expected.Top=$targetTop
 $expected.Right=$before.Right
 $expected.Bottom=$before.Bottom
 if(-not (Same-Rect $rect $expected) -and -not (Same-Rect $rect $before)){
  Write-Warning 'Another component changed the taskbar rectangle; refusing overwrite'
  return
 }
 # Even if Explorer already restored the ORIGINAL RECT, an interrupted
 # XAML cleanup could have left our window-region mask installed.
 # Only this trial's marker permits touching its optional clipping region.
 # Never clear an unrelated mod's window region.
 $mark=[AtbGeoWin32_ATB3_R1]::GetProp($trialHwnd,'ATB_OWNED_OVERHANG_V2')
 if($mark.ToInt64() -eq [int64]$PID){
  [IntPtr]$probe=[IntPtr]::Zero
  $responsive=[AtbGeoWin32_ATB3_R1]::SendMessageTimeout($trialHwnd,0,[IntPtr]::Zero,[IntPtr]::Zero,2,250,[ref]$probe)
  if($responsive -ne [IntPtr]::Zero -and
     [AtbGeoWin32_ATB3_R1]::RegionFingerprintMatches($trialHwnd)){
   # No writes unless this precise region is still the one our mod
   # published. Avoid silently removing a different Windhawk mod's region.
   # Restore the pre-trial window region, NOT nullptr when Windows or
   # another component originally supplied a region. SetWindowRgn takes
   # ownership of our snapshot only on success.
   $restore=[IntPtr]::Zero
   if($regionMode -eq 1){$restore=$baselineRegion}
   if([AtbGeoWin32_ATB3_R1]::SetWindowRgn($trialHwnd,$restore,$true) -ne 0) {
    if($regionMode -eq 1){$script:baselineRegion=[IntPtr]::Zero}
    [void][AtbGeoWin32_ATB3_R1]::RemoveProp($trialHwnd,'ATB_REGION_FINGERPRINT_V2')
    [void][AtbGeoWin32_ATB3_R1]::RemoveProp($trialHwnd,'ATB_OWNED_OVERHANG_V2')
    Write-Host '[ATB] Watchdog restored exact original taskbar window region'
   }else{
    Write-Warning 'Baseline taskbar region restoration failed; manual Explorer restart may be required'
   }
  }else{
   Write-Warning 'Window region fingerprint unverified or Explorer unresponsive; preserving external state'
   # Another component may have taken ownership: do not overwrite it.
  }
 }
 if(Same-Rect $rect $before){return}
 # SWP_ASYNCWINDOWPOS ensures an unresponsive Explorer doesn't hang watcher.
 # SWP_NOSENDCHANGING avoids Windows clamping the native shell rectangle.
 $flags=[uint32](0x4 -bor 0x10 -bor 0x400 -bor 0x4000)
 [void][AtbGeoWin32_ATB3_R1]::SetWindowPos($trialHwnd,[IntPtr]::Zero,
    $before.Left,$before.Top,
    $before.Right-$before.Left,$before.Bottom-$before.Top,$flags)
 $ok=$false
 $clock=[Diagnostics.Stopwatch]::StartNew()
 while($clock.ElapsedMilliseconds -lt 3000){
  $r=New-Object AtbGeoWin32+RECT
  if([AtbGeoWin32_ATB3_R1]::GetWindowRect($trialHwnd,[ref]$r) -and (Same-Rect $r $before)){
   $ok=$true;break
  }
  Start-Sleep -Milliseconds 40
 }
 if(-not $ok){Write-Warning 'Explorer did not restore geometry within 3 seconds; manual recovery may be required'}
 else{Write-Host '[ATB] Independent watchdog restored native window rectangle'}
}
try{
 $lock=[IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,
                      [IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 if(-not $SelfTest){
  # Read-only installed-mode inspection. Never request registry WRITE ACL.
  # The experimental local mod MUST start disabled before lease is issued.
  # After expiry its UI switch can remain on, but DLL stays inert without
  # the original Explorer PID and live, fresh ATB3 watcher lease.
  $disabled=Assert-AtbOwnModReadable
  if($disabled -ne 1){throw 'Disable the experimental local mod in Windhawk before arming'}
  Assert-NoAtbCompetingNativeDock
 }
 if($TestUnresolved) {
  if(-not $SelfTest){throw 'TestUnresolved is restricted to isolated CI self-test'}
  [IO.File]::WriteAllText($lease,'ATB3 unresolved',
                           [Text.Encoding]::ASCII)
 }
 # Fail CLOSED after a prior watchdog crash or incomplete rollback.
 # Overwriting a non-revoked transaction would silently rearm a taskbar
 # whose prior geometry/region ownership has not been reconciled.
 if([IO.File]::Exists($lease)){
  $prior=([IO.File]::ReadAllText($lease,[Text.Encoding]::ASCII)).Trim()
  if($prior -ne 'REVOKED'){
   throw 'Unresolved prior ATB3 lease: refusing to rearm. Disable the local mod and inspect the taskbar before any new trial.'
  }
 }
 if($SelfTest){
  $before=[AtbGeoWin32_ATB3_R1+RECT]@{Left=0;Top=2118;Right=3840;Bottom=2160}
  $targetTop=2052
 }else{
  $trialHwnd=[AtbGeoWin32_ATB3_R1]::FindWindow('Shell_TrayWnd',$null)
  if($trialHwnd -eq [IntPtr]::Zero){throw 'Primary native taskbar not found'}
  [uint32]$pidFound=0
  [void][AtbGeoWin32_ATB3_R1]::GetWindowThreadProcessId($trialHwnd,[ref]$pidFound)
  if($pidFound -eq 0){throw 'Explorer PID unavailable'}
  $explorerPid=$pidFound
  $r=New-Object AtbGeoWin32+RECT
  if(-not [AtbGeoWin32_ATB3_R1]::GetWindowRect($trialHwnd,[ref]$r)){throw 'Could not snapshot taskbar rectangle'}
  $before=$r
  # Take an independent, process-owned HRGN snapshot BEFORE publishing
  # any lease. ATB3 pins its content hash; the mod rejects any mismatch.
  $baselineRegion=[AtbGeoWin32_ATB3_R1]::CreateRectRgn(0,0,0,0)
  if($baselineRegion -eq [IntPtr]::Zero){
   throw 'Unable to snapshot original taskbar window region'
  }
  $regionKind=[AtbGeoWin32_ATB3_R1]::GetWindowRgn($trialHwnd,$baselineRegion)
  if($regionKind -ne 0) {
   $regionMode=1
   $regionHash=[AtbGeoWin32_ATB3_R1]::RegionHash($baselineRegion)
   if($regionHash -eq 0){throw 'Original HWND region could not be fingerprinted; refusing trial'}
  }else{
   [void][AtbGeoWin32_ATB3_R1]::DeleteObject($baselineRegion)
   $baselineRegion=[IntPtr]::Zero
   $regionMode=0
   $regionHash=[UInt64]0
  }
  Write-Host ("[ATB] Original taskbar window region: mode={0}, fingerprint={1}" -f $regionMode,$regionHash)
  $height=$before.Bottom-$before.Top
  if($height -lt 30 -or $height -gt 140){throw 'Unexpected taskbar height; fail closed'}
  $monitor=[AtbGeoWin32_ATB3_R1]::MonitorFromWindow($trialHwnd,2)
  $info=New-Object AtbGeoWin32+MONITORINFO
  $info.cbSize=[uint32][Runtime.InteropServices.Marshal]::SizeOf([type][AtbGeoWin32_ATB3_R1+MONITORINFO])
  if(-not [AtbGeoWin32_ATB3_R1]::GetMonitorInfo($monitor,[ref]$info)){throw 'Monitor unavailable'}
  if($before.Bottom -ne $info.monitor.Bottom){throw 'Only bottom-aligned primary taskbar supported'}
  $baselineWork=$info.work
  $dpi=[AtbGeoWin32_ATB3_R1]::GetDpiForWindow($trialHwnd)
  if($dpi -lt 96 -or $dpi -gt 480){throw 'DPI outside supported bounds'}
  $targetHeight=[Math]::Max($height+48,[Math]::Ceiling(108*$dpi/96.0))
  $targetHeight=[Math]::Min($targetHeight,$height+150)
  if($targetHeight -ge ($before.Bottom-$info.monitor.Top)){throw 'Insufficient monitor vertical space'}
  $targetTop=[int]($before.Bottom-$targetHeight)
 }
 $createdUtc=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
 $deadlineUtc=$createdUtc+$durationMs
 $watch=[Diagnostics.Stopwatch]::StartNew()
 Write-Host ("[ATB] Geometry trial armed, Explorer PID $explorerPid; expiration $durationMs ms")
 try{
  while($watch.ElapsedMilliseconds -lt $durationMs) {
   if(-not $SelfTest){
    $h=[AtbGeoWin32_ATB3_R1]::FindWindow('Shell_TrayWnd',$null)
    [uint32]$p=0
    if($h -ne $trialHwnd){break}
    [void][AtbGeoWin32_ATB3_R1]::GetWindowThreadProcessId($h,[ref]$p)
    if($p -ne $explorerPid){break}
   }
   $now=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
   if($now -ge $deadlineUtc){break}
   Write-Lease ("ATB3 {0} {1} {2} {3} {4} {5} {6} {7} {8} {9} {10}`n" -f
     $explorerPid,$deadlineUtc,$now,$PID,
     $before.Left,$before.Top,$before.Right,$before.Bottom,$targetTop,
     $regionMode,$regionHash)
   Start-Sleep -Milliseconds 200
  }
 }finally{
  # Do NOT write to Windhawk's protected HKLM registry. Revoking the
  # lease makes any already-loaded mod disarm and prevents reapplication.
  # On a new Explorer, the exact Explorer PID in the old lease cannot match.
  # Windhawk's UI switch can remain enabled; user may turn it off manually.
  try { Write-Lease "REVOKED`n" }
  finally { Stop-NativeTrial }
  Write-Host '[ATB] Trial expired and lease revoked; native mod is inert.'
  if(-not $SelfTest){
   Write-Host '[ATB] If Windhawk still shows the trial mod Enabled, manually disable it in Windhawk.'
  }
 }
}finally{
 if($baselineRegion -ne [IntPtr]::Zero){
  [void][AtbGeoWin32_ATB3_R1]::DeleteObject($baselineRegion)
 }
 if($lock){$lock.Dispose()}
}
