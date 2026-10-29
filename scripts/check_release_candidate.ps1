# Stage 1i: release-only FAIL-CLOSED check. NEVER signs/installs/enables a mod.
# The committed policy is unapproved and will therefore reject everything.
# This check is necessary, NOT sufficient, for a future production release:
# protected approvals, approved signer identity and exact watchdog build remain required.
param(
    [Parameter(Mandatory = $true)][string]$Candidate,
    [Parameter(Mandatory = $true)][string]$PolicyFile,
    [Parameter(Mandatory = $true)][string]$SourceCommit
)
$ErrorActionPreference = 'Stop'
try {
    $policy = Get-Content -LiteralPath $PolicyFile -Raw | ConvertFrom-Json
    $keys = @($policy.PSObject.Properties.Name | Sort-Object)
    $expectedKeys = @(@('approved','artifact_name','schema','sha256','signer_thumbprint','source_commit','target') | Sort-Object)
    if (@(Compare-Object $keys $expectedKeys).Count -ne 0) { throw 'Unknown or missing release-policy keys' }
    if ($policy.schema -ne 1 -or $policy.target -cne 'atb-native-watchdog') { throw 'Unexpected release-policy target/version' }
    if ($policy.approved -isnot [bool] -or $policy.approved -ne $true) { throw 'Watchdog release is NOT APPROVED' }
    if ([IO.Path]::GetFileName($Candidate) -cne 'atb-watchdog.exe' -or
        $policy.artifact_name -cne 'atb-watchdog.exe') { throw 'Wrong artifact identity' }
    if ($SourceCommit -cnotmatch '^[0-9a-f]{40}$' -or $policy.source_commit -cne $SourceCommit) {
        throw 'Source SHA not independently approved'
    }
    if ($policy.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Missing approved full-file SHA256' }
    if ($policy.signer_thumbprint -cnotmatch '^[0-9A-F]{40}$') { throw 'Missing approved signer certificate SHA1 thumbprint' }
    if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) { throw 'Missing candidate binary' }
    $hash = (Get-FileHash -LiteralPath $Candidate -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -cne $policy.sha256) { throw 'Full-file SHA256 mismatch' }
    $signature = Get-AuthenticodeSignature -LiteralPath $Candidate
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
        $null -eq $signature.SignerCertificate) { throw 'Unsigned or untrusted Authenticode signature' }
    if ($signature.SignerCertificate.Thumbprint.ToUpperInvariant() -cne $policy.signer_thumbprint) {
        throw 'Signer does not match the approved certificate'
    }
    Write-Output 'Candidate has the approved digest and valid approved Authenticode signer. Additional runtime safety gates remain.'
    exit 0
} catch {
    [Console]::Error.WriteLine("RELEASE BLOCKED: $($_.Exception.Message)")
    exit 3
}
