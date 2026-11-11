# EXP-007 — XRIPC second-session probe (GATED, do not run casually)

- Date: 2026-11-11 (planned, **needs explicit permission**)
- Frame build: SteamVR 2026-09-23 23:30:05
- Hardware in play: Frame only
- Status: planned — blocked on user go/no-go

## Why this exists

`docs/08` argues a second XRIPC client is structurally impossible: `XRService` publishes a
fixed set of twelve shared-memory mailboxes in `/dev/shm` with no per-client indexing
(`XR_ClientRequest_{VeryLow,Low,Mid,High}_Data` and companions), one mutex per latency class
(`XRIPCPeer::clientFunctionCallLock_*`), and an explicit
`XRIPCSession::isThereASessionRunningInAnotherProcess()` guard.

That argument is strong but it is inference from artefacts. This experiment turns it into a
measured fact — **and it is the one experiment where being wrong damages a live session.**

## Hypothesis

Calling `PE::XRIPCSession::startSession("sfct-probe")` from a second process while `XRService`
and `driver_cv.so` are live will fail cleanly (the `isThereASessionRunningInAnotherProcess`
guard, or "Client is already connected to the service"), leaving tracking intact.

If instead it succeeds, the mailboxes are shared and the second client can corrupt the first.

## Preconditions

1. User has said yes, out loud, in this conversation.
2. SteamVR is **not** in active use (nobody wearing the headset).
3. Backup taken of `~/.config/openvr/config/cv/xrservice/` and `~/.config/openvr/config/roy/`.
4. A recovery path written down: `steamvr fixup` (the binary itself references it:
   `initializeSharedMem: shm_open (%s) ERR %d (%s). This probably means you need to run
   "steamvr fixup" to remove stale shared memory files.`), plus a full SteamVR restart.
5. `vrserver.txt` and the `XRService` log tailed live during the run.

## Procedure

1. Record baseline: `vrcmd --info`, one `trackingtest` sample, `ls -l /dev/shm/XR_*`.
2. Compile a minimal probe that links `libArcturusPerception.so` and calls, in order:
   `isThereASessionRunningInAnyProcess()`, `isTheServiceProcessCurrentlyRunning()`,
   `initializeSharedMemoryWithService(false)`, `startSession("sfct-probe")`.
3. Print the result of each, then exit immediately. **No property writes. No `setDynamicOptions`.
   No `setDatasetRecordingEnabled`.** This probe only asks "will you talk to me?".
4. Re-run `vrcmd --info` and `trackingtest`. Compare against baseline.
5. Check logs for new errors.

## Abort criteria

Any of: tracking validity drops, `vrserver.txt` reports device loss, `XRService` logs an IPC
fault, or `trackingtest` stops updating. On abort: kill probe, restart SteamVR, run
`steamvr fixup` if mailboxes are stale.

## Result

Pending.

## Conclusion

Pending.

## Next

- If clean success: a **read-only** metrics client becomes plausible
  (`getTrackingMetrics`, `getControllerTrackingMetrics`, `getNumericalProperty`) — but only for
  reads, and only if it can prove it does not write the request buffer.
- If clean failure: `docs/08` §3 stands as measured fact, and layer 1 (OpenVR client) is the
  permanent answer.
