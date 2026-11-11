# Roadmap

Phases are gated: don't start the next one until the previous one has evidence in `notes/`.

---

## P0 — Understand (current)

**Goal:** know the Frame's controller tracking and pairing stack well enough to predict
what will and won't work.

- P0.1 Inventory the on-device stack: SteamVR binaries, services, configs, tracking-related
  processes, log locations. `[x]` — see `docs/07`
- P0.2 Capture the Frame's own controller behaviour as baseline: LED blink pattern, pairing
  flow, what the tracking dataset contains. `[ ]`
- P0.3 Answer the "how many controllers" question with at least static evidence and a test
  plan. `[~]` — static evidence collected, no numeric ceiling found; test plan open
- P0.4 Find out whether any debug view of the tracking cameras with marker overlay exists,
  and whether "filters" can be toggled. `[~]` — "Show Debug Camera Views" toggle found; not yet enabled

**Gate:** we can explain, in writing, every stage between "LED blinks" and "pose in OpenXR".

---

## P1 — See (optical observation rig)

**Goal:** reliably capture what the tracking cameras see, with and without processing.

- P1.1 Headset view capture over `/dev/video99` at good resolution. `[ ]`
- P1.2 Tracking dataset recording with camera video enabled (Valve's own dev feature). `[ ]`
- P1.3 Independent IR observation rig (IR-pass camera / phone camera) for controllers the
  Frame isn't tracking. `[ ]`
- P1.4 Frame the LED constellation geometry of the Frame controller from captures. `[ ]`

**Gate:** we can show a still frame where individual LEDs are visible and countable.

---

## P2 — Detect (custom constellation recognition)

**Goal:** our own code identifies LED blobs in a camera frame and estimates a rigid-body pose
for *any* rigid body we stick LEDs on.

- P2.1 Blob detector on captured frames, offline first. `[ ]`
- P2.2 Constellation model + pose solve (PnP) for one known object. `[ ]`
- P2.3 Live pipeline at camera rate on the Frame (or on the PC host). `[ ]`
- P2.4 Fuse with IMU. `[ ]`

**Gate:** a rig of LEDs on a stick produces a stable pose we can watch move.

---

## P3 — Bridge (Quest 3 controllers)

**Goal:** Quest 3 controllers produce usable input and pose on the Frame.

- P3.1 Radio reconnaissance: BLE advertising, 2.4 GHz activity, pairing-mode behaviour. `[ ]`
- P3.2 Optical reconnaissance: do Touch Plus LEDs emit when unpaired / in pairing mode? `[ ]`
- P3.3 Reproduce Meta's 8-LED constellation model on their LEDs. `[ ]`
- P3.4 Input path: get buttons/IMU off the radio, or accept pose-only. `[ ]`
- P3.5 Present the result to SteamVR/OpenXR as a tracked device. `[ ]`

**Gate:** a Quest 3 controller moves a virtual object in a Frame app.

---

## P4 — Far goal (Index controllers with added tracking points)

**Goal:** Index controllers tracked by the Frame's cameras.

Important constraint: the Index controller is a *lighthouse sensor* device — it has
photodiodes, not emitters, and the Frame does not support base stations `[P]`. So this is not
"pair an Index controller", it is "build a Frame-visible emitter rig on an Index controller
and drive its input separately."

- P4.1 Physical rig: LED carrier mounted to the Index controller, battery, no occlusion of
  the controller's own sensors (they'll be unused in this mode, but keep it reversible). `[ ]`
- P4.2 Reuse P2 pose pipeline with the rig's own constellation. `[ ]`
- P4.3 Input path for Index buttons/trigger/trackpad (USB or wireless). `[ ]`
- P4.4 Fuse rig pose + native input into one OpenXR device. `[ ]`

**Gate:** Index controller plays a Frame-native title with pose and buttons both working.

---

## Parked / open

- Hand tracking on the Frame: not announced `[P]`; Quest 3 fuses hand tracking with controller
  LEDs, the Frame may not. Affects how gracefully Touch Plus occlusion degrades.
- Lighthouse support on Frame: reported as not supported at launch `[P]`. Re-verify on current
  build before spending money on base stations.
- Multi-controller ceiling: see `docs/04-tracking-limits.md`.
