# Stage 1e — Windows-native IPC authentication probe (diagnostic only)

This stand-alone Windows-only executable tests named-pipe authentication; it is **not wired into Windhawk, Explorer, the taskbar, or the existing mock watchdog**. No OS settings are modified. It is a protocol and Windows API feasibility gate, not a production recovery authority.

## Protocol

The server creates a local named pipe with a random suffix under the ATB-MOCK-IPC prefix, with a DACL limited to the current user SID and SYSTEM and with PIPE_REJECT_REMOTE_CLIENTS. It reads a HELLO message, obtains the kernel-reported client PID and session, impersonates the connected client, and compares its Windows token SID with the server's SID. Cross-session and mismatched identities are refused.

The server sends a CNG-generated random challenge containing a 256-bit nonce, a separate random 128-bit trial ID, and a boot-monotonic timestamp. It authenticates itself with HMAC-SHA256 over the challenge under a server-specific domain separator. The client verifies that tag before replying with a different client-domain HMAC-SHA256. The challenge is valid for five seconds; a response captured from an earlier trial cannot be reused against the next random nonce. MAC comparisons use a fixed-size bytewise comparison.

A fresh 256-bit test secret is supplied to each standalone process on **stdin**, not command-line arguments or a journal. The client-capture and client-replay modes exist **only** to test replay rejection. They are not intended for a production agent. The server has no taskbar, installer, Windhawk, or Explorer mutation commands.

## Build/test

The PR's GitHub Actions workflow builds this diagnostic on an isolated Windows runner with C++17 g++ and links the native bcrypt and advapi32 libraries. It runs tests/test_native_ipc.py with subprocess timeouts. The portable protocol contract also compiles on Windows and Ubuntu. No installer or auto-start entry is produced.

## Remaining blocking security gates

- **Blocking I/O is not sufficiently bounded:** named-pipe operations are synchronous and can stall if a peer hangs. The test harness enforces subprocess timeouts, not production-safe overlapped I/O cancellation.
- Same-user SID, session, PID and a shared secret do not prove a particular **trusted executable**. The client verifies the server's cryptographic proof but not the server executable signature/path or PID. A process able to obtain the secret can authenticate.
- Bootstrap and rotation of the secret between the actual Explorer mod and watchdog have not been designed or tested. The existing read-only lease.v1 file is not authorization.
- There is no production watchdog service, Windows-startup reconciliation, Windhawk version-specific automatic disable, or durable rollback of actual taskbar state.
- Sleep, power failure or simultaneous failure of all recovery processes prevent an unconditional software-only 120-second wall-time recovery guarantee.

**Stage 1 remains BLOCKING.** Do not install or enable taskbar-changing builds.

## Stage 1f — bounded transport and independent server-identity checks

The standalone IPC diagnostic now uses FILE_FLAG_OVERLAPPED on both ends of the named pipe. A single 3-second monotonic handshake deadline covers connecting, HELLO, challenge, HMAC proof and acknowledgement. For pending operations, GetOverlappedResultEx waits only for the remaining budget; on expiry it calls CancelIoEx and drains the outstanding overlapped operation before freeing its buffer. The last drain may still be delayed by the OS; the GitHub test subprocess has a separate hard timeout. Synchronous FlushFileBuffers, which could wait for a stalled client, is intentionally removed.

The client additionally queries the actual pipe server PID, opens that process, checks it is still running, compares the Windows user SID and session, and checks that the server's executable path matches this diagnostic client's executable. The test harness provides the expected server PID, which is **not a secret**. This same-executable check is valid for the standalone self-test only; the real watchdog and Explorer will be different programs and will require independently verified signing and binary identity policies.

Failure-injection tests now cover a missing client, an intentionally silent server, and a mismatched expected server PID, in addition to earlier wrong-secret and replay attacks.

**No Windhawk integration is implied by this:** neither a credential nor an authenticated IPC client has yet been installed inside the read-only Windhawk mod. The existing mock watchdog is still Python. A true Explorer/watchdog bridge needs protected session-scoped secret handoff and a Windows-native watchdog implementation, not a hardcoded token in Windhawk settings.

## Stage 1g — Windhawk diagnostic source bridge

A new source header, src/core/diagnostic_ipc_win.hpp, defines a **read-only** peer probe reusing the Stage 1f bounded named-pipe transport. The existing native test executable has an observer-server mode that accepts a static 8-byte diagnostic hello after Windows SID/session checks and replies with a static ack. Tests compile a separate host directly from the same header that is bundled into the Windhawk diagnostic source.

All three Windhawk diagnostic peer settings default to off. Test mode does not use a secret and cannot authorize mutations. Source bundling is CI syntax-checked using stub API declarations, not installed or loaded into Explorer.

**Security constraint:** PID, same-user SID/session and an executable path are useful *diagnostic assertions*, not a trusted binary identity. A replaceable same-user image and an unauthenticated static response are not sufficient for a production watchdog channel. Production release remains blocked pending a signed/pinned watchdog identity policy, protected key bootstrap, native recovery, and verified rollback.
