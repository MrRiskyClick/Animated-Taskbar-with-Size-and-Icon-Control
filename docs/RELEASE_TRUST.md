# Stage 1i — Build provenance and blocked release gate

The Stage 1h Windows identity diagnostic now has a verifiable **test-build record** and a separate **release-approval gate**. These are distinct; producing a CI artifact does **not** mean it is trusted, signed, installed, released or suitable for Explorer.

## What CI does

On the existing Windows test job, after the native identity tests pass:

1. Tests that `scripts/check_release_candidate.ps1` refuses the committed unapproved policy, an unsigned test binary even when given a fabricated approval, and a binary modified after its SHA-256 was calculated.
2. Generates `untrusted-manifest.json` for the **diagnostic executable only**, including Git commit SHA and binary SHA-256. Its `trusted`, `signed`, `deployable` and `release_approved` values are always `false`.
3. Archives the unsigned test binary and manifest as a short-lived **UNTRUSTED-identity-diagnostic** GitHub Actions artifact (three-day retention). This upload is *only* storage within the Actions run. **It is not a deployment, installer, GitHub Release, signing job, startup entry, or modification to any PC.**

No new workflow trigger is added. Normal feature-branch work remains tested via pull requests, while direct `main` pushes also run tests.

## Current production policy

`release/watchdog-approval.json` is intentionally blocked:

- `approved: false`
- no approved source commit
- no approved executable SHA-256
- no approved signing certificate thumbprint
- the future release target is `atb-watchdog.exe`, **which does not yet exist**

The PowerShell release check fails unless every mandatory field is present, the candidate is the exact expected filename, the source commit matches, the full-file digest matches, Windows Authenticode reports **Valid**, and the exact certificate thumbprint matches. It never signs, installs, enables, disables, modifies or starts any mod.

**Warning:** The certificate SHA-1 thumbprint is only a conventional Windows certificate *identifier*, not the artifact's integrity hash. The full file is independently SHA-256 verified. A certificate identity and Windows trust do not by themselves attest that process memory is unmodified.

## A passing release check will not be sufficient on its own

Before any *actual* production release, the following independent controls are required:

- A finished Windows-native watchdog executable that owns the recovery design, plus a tested installation/uninstallation and Windows-startup reconciliation mechanism.
- A reviewed signing identity and certificate renewal/revocation procedure. The project has no signing certificate or approved signed artifact yet. **Do not embed a signing private key in GitHub Actions, the repository, or Windhawk settings.**
- Independent approval of a specific *signed* binary SHA-256 and signer; maintainers should protect the approving branch and require review. The currently unprotected or development branch is **not a trust anchor**.
- A secure, authenticated IPC binding between the signed watchdog, its session and the actual Windhawk mod.
- Durable reversible changes to individual taskbar properties, strict 120-second trial expiry, safe mod disable/no-reapply, boot/crash recovery, and failure injection on a disposable Windows environment.
- Real Windows Authenticode signing is not performed in this CI. Unsigned diagnostic artifacts must never be relabeled into release candidates by changing JSON metadata.

## Test results and interpretation

Windows negative-path tests exercise unapproved, unsigned and tampered files. Linux validates the untrusted manifest and deliberately blocked policy; it does not run Windows Authenticode. No positive test with an approved signed release is possible until a legitimate signed watchdog binary exists.

**Stage 1 full safety gate remains OPEN. No active Windows taskbar changes are authorized.**
