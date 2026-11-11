# Experiments

One file per experiment. Copy the template, give it a number, never reuse a number.

Template:

```
# EXP-NNN — title

- Date:
- Frame build / SteamOS build:
- Hardware in play:
- Status: planned | running | passed | failed | inconclusive

## Hypothesis
## Procedure
## Result
## Evidence
## Conclusion (with tag)
## Next
```

## Index

| ID | Title | Status |
|---|---|---|
| EXP-001 | Quest 3 controllers: do they emit IR when unpaired? | planned |
| EXP-002 | Tracking ceiling: does the Frame notice a third emitter? | planned |
| EXP-003 | Tracking dataset with camera video: where it lands, what's in it | planned |
| EXP-004 | Frame controller constellation extracted from headset model files | **passed 2026-11-11** |
| EXP-005 | Bluetooth visibility of Quest 3 controller in pairing mode | planned |
| EXP-006 | Custom OpenVR diagnostic client `sfct-diag` (read-only pose census) | planned |
| EXP-007 | XRIPC second-session probe — **gated, needs explicit permission** | planned |

Rules:

- A failed experiment is a result. Record it.
- If a procedure touches the device's system state, note exactly how to undo it.
- Captures go to `captures/` (git-ignored). Conclusions go in the file.
