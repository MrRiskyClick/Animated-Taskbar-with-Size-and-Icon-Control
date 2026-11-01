# ATB Desktop Updater - versioned builds, safe replacement, verification.
# Use in ordinary (non-elevated) PowerShell. Requires authenticated GitHub CLI.
# Stops only running ATB Dock processes, never Explorer or Windhawk.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repo = 'MrRiskyClick/Animated-Taskbar-with-Size-and-Icon-Control'
$desktop = [Environment]::GetFolderPath('Desktop')
$temp = Join-Path $env:TEMP ('ATB-Download-' + [guid]::NewGuid().ToString('N'))

function Gh-Json([string[]]$Arguments) {
    $result = & gh @Arguments
    if ($LASTEXITCODE -ne 0) { throw ('GitHub CLI command failed: gh ' + ($Arguments -join ' ')) }
    return ($result | Out-String | ConvertFrom-Json)
}

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw 'GitHub CLI (gh) is required. The previous updater used gh as well.'
}

Write-Host 'Checking PR #24 for a successfully compiled, versioned build...'
$pr = Gh-Json @('pr','view','24','--repo',$repo,'--json','headRefOid,headRefName')
if (-not $pr.headRefOid -or -not $pr.headRefName) {
    throw 'Unable to determine the exact PR head commit. Nothing has changed on Desktop.'
}
$runs = @(Gh-Json @('run','list','--repo',$repo,'--workflow','rust-dock-ci.yml',
                    '--branch',$pr.headRefName,'--limit','50',
                    '--json','databaseId,status,conclusion,headSha'))
$run = $runs | Where-Object {
    $_.headSha -eq $pr.headRefOid -and
    $_.status -eq 'completed' -and
    $_.conclusion -eq 'success'
} | Select-Object -First 1
if (-not $run) {
    throw "The current PR commit $($pr.headRefOid.Substring(0,8)) does not have a successful build yet. Existing desktop EXE and folder were NOT modified."
}

New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    & gh run download $run.databaseId --repo $repo --dir $temp
    if ($LASTEXITCODE -ne 0) {
        throw 'GitHub artifact download failed. Existing desktop files were NOT modified.'
    }
    $infoFiles = @(Get-ChildItem -LiteralPath $temp -File -Filter 'build-info.json' -Recurse)
    if ($infoFiles.Count -ne 1) {
        throw 'Expected exactly one signed-off build-info.json file in downloaded artifact.'
    }
    $infoFile = $infoFiles[0]
    $info = Get-Content -LiteralPath $infoFile.FullName -Raw | ConvertFrom-Json
    if ($info.displayVersion -notmatch '^v\d+(?:\.\d+){0,2}$') {
        throw 'Invalid packaged display version.'
    }
    $expectedName = 'ATB-Dock-' + $info.displayVersion + '.exe'
    if ($info.executable -cne $expectedName) {
        throw "Manifest EXE name does not match version: $($info.executable)"
    }
    if ($info.gitSha -cne $pr.headRefOid) {
        throw "Artifact commit $($info.gitSha) differs from current PR commit $($pr.headRefOid)."
    }
    if ([string]$info.workflowRunId -ne [string]$run.databaseId) {
        throw 'Artifact run ID does not match the selected successful GitHub build.'
    }
    $downloadExe = Join-Path $infoFile.DirectoryName $expectedName
    if (-not (Test-Path -LiteralPath $downloadExe -PathType Leaf)) {
        throw "Versioned EXE is missing from artifact: $expectedName"
    }
    $actualHash = (Get-FileHash -LiteralPath $downloadExe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne ([string]$info.sha256).ToLowerInvariant()) {
        throw 'Downloaded EXE SHA-256 differs from the build manifest. Installation aborted.'
    }

    $newFolder = Join-Path $desktop ('ATB-Dock-' + $info.displayVersion)
    $legacyFolders = @(Get-ChildItem -LiteralPath $desktop -Directory -ErrorAction Stop |
        Where-Object { $_.Name -match '^ATB-Dock-v(?:0\.)?\d+(?:\.\d+)*$' -and $_.FullName -ne $newFolder } |
        Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName 'atb-rust-dock.exe')) -or
            (Test-Path -LiteralPath (Join-Path $_.FullName 'build-info.json')) -or
            @((Get-ChildItem -LiteralPath $_.FullName -File -Filter 'ATB-Dock-v*.exe')).Count -gt 0
        } | Sort-Object LastWriteTime -Descending)

    # The verified new executable is safely staged before stopping anything.
    $atbProcesses = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^(?:atb-rust-dock|ATB-Dock-v\d+(?:\.\d+)*)$'
    })
    foreach ($process in $atbProcesses) {
        Stop-Process -Id $process.Id -Force -ErrorAction Stop
    }

    # Migrate the old desktop directory instead of leaving misleading v0.2 names.
    if (-not (Test-Path -LiteralPath $newFolder) -and $legacyFolders.Count -gt 0) {
        Move-Item -LiteralPath $legacyFolders[0].FullName -Destination $newFolder -ErrorAction Stop
    }
    New-Item -ItemType Directory -Path $newFolder -Force | Out-Null
    $target = Join-Path $newFolder $expectedName
    Copy-Item -LiteralPath $downloadExe -Destination $target -Force -ErrorAction Stop
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $actualHash) {
        throw 'Installed EXE failed the SHA-256 check.'
    }
    Copy-Item -LiteralPath $infoFile.FullName -Destination (Join-Path $newFolder 'build-info.json') -Force

    # Only clear superseded ATB exe names inside the new installation directory.
    @(Get-ChildItem -LiteralPath $newFolder -File -Filter '*.exe') |
        Where-Object { $_.Name -ne $expectedName -and
            ($_.Name -eq 'atb-rust-dock.exe' -or $_.Name -match '^ATB-Dock-v[\d.]+\.exe$') } |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }

    # If an old version directory still exists, remove it ONLY when it contains
    # application-managed EXE/build-info files and nothing belonging to the user.
    foreach ($old in $legacyFolders) {
        if (-not (Test-Path -LiteralPath $old.FullName) -or $old.FullName -eq $newFolder) { continue }
        $contents = @(Get-ChildItem -LiteralPath $old.FullName -Force)
        $unmanaged = @($contents | Where-Object {
            $_.PSIsContainer -or
            ($_.Name -ne 'atb-rust-dock.exe' -and $_.Name -ne 'build-info.json' -and
             $_.Name -notmatch '^ATB-Dock-v[\d.]+\.exe$')
        })
        if ($unmanaged.Count -eq 0) {
            Remove-Item -LiteralPath $old.FullName -Recurse -Force
        } else {
            Write-Warning "Old folder kept (contains other files): $($old.FullName)"
        }
    }

    Write-Host ''
    Write-Host "Installed: $target" -ForegroundColor Green
    Write-Host "Version: $($info.displayVersion) (Rust $($info.packageVersion))"
    Write-Host "GitHub commit: $($info.gitSha)"
    Write-Host "Successful GitHub Actions run: $($run.databaseId)"
    Write-Host 'Verification: SHA-256 MATCH' -ForegroundColor Green
    Write-Host "Build record: $(Join-Path $newFolder 'build-info.json')"
}
finally {
    if (Test-Path -LiteralPath $temp) {
        Remove-Item -LiteralPath $temp -Force -Recurse -ErrorAction SilentlyContinue
    }
}
