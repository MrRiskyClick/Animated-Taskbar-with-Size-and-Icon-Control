# Stage 1j — Native Windows mock watchdog (not a production recovery agent)

This milestone introduces a **Windows-native C++17 watchdog executable** and a separate portable trial-state contract. The executable is named `atb-native-mock-watchdog.exe` to prevent confusion with the **nonexistent** signed production `atb-watchdog.exe`.

Its only writable targets are `target.v1`, `journal.v1`, `state.lock` and temporary atomic-write files inside a specially named **Windows temporary directory** (`atb-native-mock-...`). It does **not** call Explorer, register a service, alter a taskbar, change Windhawk settings or touch the registry. CI compiles and runs it on disposable GitHub-hosted Windows runners; it is not published or installed on the user's computer.

## Native functionality

- **init:** creates an isolated 16–160-height mock fixture, refusing to overwrite prior state.
- **arm:** persists a unique random trial ID, original and proposed mock values, and both UTC and monotonic deadlines. The normal maximum period is 120 seconds. Short test trials require `ATB_NATIVE_MOCK_TEST_MODE=1`.
- **run:** a separate native watchdog process acquires an exclusive file lock, marks the trial ready, and publishes a heartbeat. It monitors deadlines independently of the parent Python test harness.
- **apply:** a separate command verifies the ready state, trial ID, fresh heartbeat and live watcher PID before editing **only the mock target**. It refuses unrelated edits.
- **automatic timeout:** the native watchdog compares current mock state and restores the baseline when the trial expires. External edits cause `CONFLICT`, never a blind overwrite.
- **recover:** explicit manual reconciliation, including when the watchdog has been killed.
- **reconcile:** fail-closed stale/expired trial reconciliation, without automatically creating or rearming a new trial.
- **status:** reads the mock trial and target under the same lock.

The native core strictly rejects corrupted/oversized/unknown journal and target records. Writes use a same-directory temporary file, flush and atomic replacement. An exclusive Windows lock is released when the owning process dies.

## Safety and test boundaries

Windows CI deliberately exercises normal expiry, a killed native watchdog, missed heartbeat, third-party changes, malformed journal, incorrect trial IDs, refusal to run short trials without mock-mode authorization, no silent rearm, and rejection of directories outside the specially named TEMP sandbox. Portable C++ core tests also run on Linux.

**This is not yet reliable real-Windows-taskbar protection.** The watcher can still die, and **there is no installed supervisor or Windows boot/startup recovery**. If the watcher dies, native `reconcile` is available only when explicitly invoked; it is not automatically scheduled. Both processes can stop or Windows can be suspended. A watchdog loop and a mock-file journal do not by themselves guarantee restoration within two minutes in those cases.

Other production-blocking gaps: the native executable is unsigned/unapproved, not connected to the authenticated IPC proof of concept or Windhawk; the present mock journal/heartbeat is not a trusted authorization mechanism; no actual Windows taskbar property snapshot, reverse operation or Windhawk disable has been implemented. No live Windows taskbar modifications are authorized.

## Reproducible test steps (not an installation)

After compiling with Windows g++:

```powershell
$env:ATB_NATIVE_MOCK_TEST_MODE='1'
$state = Join-Path $env:TEMP 'atb-native-mock-example'
./atb-native-mock-watchdog.exe init $state 44
./atb-native-mock-watchdog.exe arm $state 70 5
# Use the printed TRIAL=<id> in subsequent commands:
# Start "run <state> <id>" as a separate native process before "apply".
# The native process restores the original mock target after its deadline.
```

These are **mock-only testing commands**; the user does not need to execute them.

**Full Stage 1 safety gate: NOT PASSED.**
