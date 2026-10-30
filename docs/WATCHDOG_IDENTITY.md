# Stage 1h — Windows watchdog binary identity policy (diagnostic only)

## Implemented

The standalone C++17 verifier in `native/watchdog_identity_win.hpp` checks:

1. **Exact full-file SHA-256** against a 32-byte digest supplied by a policy. It reads the file in bounded chunks using Windows CNG while holding a read-only file handle that does not share writes/deletion.
2. **Windows Authenticode trust**, using `WinVerifyTrust` and `WINTRUST_ACTION_GENERIC_VERIFY_V2`, with no interaction/UI. The strict policy requires this check **in addition to** the full-file digest. Signature verification uses cache-only certificate URL retrieval and fails closed when the necessary trust/revocation information is unavailable.
3. **Process identity**: checks that the PID still exists, verifies its recorded Windows process-creation timestamp, queries the full image path, evaluates the file under the policy, and checks the same process creation timestamp again.

The test CLI in `tests/binary_identity_probe_win.cpp` supports a deliberately **unsafe unsigned-fixture mode** for locally built GitHub Actions test executables. CI tests verify acceptance of the correct exact digest in unsigned test mode, refusal after file tampering, wrong digests, missing files, malformed policy arguments, strict signature rejection of unsigned test artifacts, wrong PID and stale creation time.

**None of this is wired into Windhawk/Explorer or the watchdog.** It is standalone read-only verification on an isolated Windows CI worker. No executable is installed or deployed to a user machine.

## Production security requirements still NOT satisfied

- An approved watchdog binary **does not yet exist**. A trusted expected SHA-256 digest must be pinned through a reviewed build/release process, not passed as user-editable Windhawk settings, taken from an unauthenticated local file, or supplied by the same untrusted process being verified.
- A Windows-trusted Authenticode signature means that **some** signer passes Windows trust policy. It does not necessarily mean it is **our** intended publisher. An approved release policy must pin the expected publisher certificate identity, subject to an explicit rotation policy, **as well as** the exact digest.
- Hashing the executable at the process's reported path does not prove that its **already loaded process memory** matches the disk file; file replacement, executable masquerading, process injection and time-of-check/time-of-use issues require a stronger threat-model decision.
- The verifier is not itself inside an authenticated IPC handshake; a matching PID/path is still **not** permission to edit the taskbar. Per-trial secret bootstrap, channel authentication, replay protection, signed native watchdog deployment, startup recovery and real per-property rollback remain incomplete.
- Signature verification inside a timed shell callback may be expensive or blocked by OS trust services. A future native watchdog should perform expensive checks outside Explorer and supply a fresh independently authenticated attestation.
- A software watchdog still cannot restore an unbootable/offline Windows system within two minutes; system recovery documentation is a secondary fallback, not a hard guarantee.

## Sources

- Microsoft WinVerifyTrust documentation: https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust
- Microsoft WINTRUST_DATA documentation: https://learn.microsoft.com/en-us/windows/desktop/api/wintrust/ns-wintrust-wintrust_data

**Stage 1 safety gate remains OPEN; the real taskbar must remain untouched.**

## Stage 1i — release and approval are now separate

The [release trust policy](RELEASE_TRUST.md) adds a completely blocked approval record (`release/watchdog-approval.json`) and a Windows PowerShell gate that checks exact source commit, file SHA-256, Authenticode trust and certificate thumbprint against independently approved values. No certificate or trusted watchdog binary is available. CI archives only an **UNTRUSTED** unsigned diagnostic with explicit provenance; this archive is not a release.

Do not permit the current unprotected feature branch or an unreviewed manifest to provide the production trust anchor. Real authenticated watchdog IPC and Windows restore remain incomplete.
