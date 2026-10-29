# Deliberate negative tests. Never signs, releases, or installs the test binary.
param([Parameter(Mandatory=$true)][string]$UnsignedDiagnostic,
      [Parameter(Mandatory=$true)][string]$SourceCommit)
$ErrorActionPreference='Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $root 'scripts/check_release_candidate.ps1'
$blocked = Join-Path $root 'release/watchdog-approval.json'
$temp = Join-Path $env:RUNNER_TEMP 'atb-release-gate-negative-tests'
New-Item -Path $temp -ItemType Directory -Force | Out-Null
try {
    $fakeCandidate = Join-Path $temp 'atb-watchdog.exe'
    Copy-Item -LiteralPath $UnsignedDiagnostic -Destination $fakeCandidate -Force
    $hash = (Get-FileHash -LiteralPath $fakeCandidate -Algorithm SHA256).Hash.ToLowerInvariant()
    $fakePolicy = Join-Path $temp 'fake-approved-test-policy.json'
    $fake = [ordered]@{
        schema=1; target='atb-native-watchdog'; approved=$true;
        artifact_name='atb-watchdog.exe'; source_commit=$SourceCommit;
        sha256=$hash; signer_thumbprint=('A' * 40)
    }
    $fake | ConvertTo-Json | Set-Content -LiteralPath $fakePolicy -Encoding utf8

    & $verifier -Candidate $fakeCandidate -PolicyFile $blocked -SourceCommit $SourceCommit
    if ($LASTEXITCODE -ne 3) { throw 'Unapproved policy was not blocked' }
    & $verifier -Candidate $fakeCandidate -PolicyFile $fakePolicy -SourceCommit $SourceCommit
    if ($LASTEXITCODE -ne 3) { throw 'Unsigned diagnostic was accepted under fake release approval' }
    Add-Content -LiteralPath $fakeCandidate -Value 'tampered'
    & $verifier -Candidate $fakeCandidate -PolicyFile $fakePolicy -SourceCommit $SourceCommit
    if ($LASTEXITCODE -ne 3) { throw 'Digest-mismatched candidate was accepted' }
    Write-Output 'All release-gate negative tests passed. No release was approved.'
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
