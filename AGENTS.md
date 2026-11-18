# AGENTS.md — Steam Frame Controller Tracking

Onboarding note for any agent (human or machine) arriving in this repository. Read this
before touching code, the headset, or the docs. When in doubt, prefer the verified note over
the tidy theory, and prefer the read-only path over the clever one.

---

## 0. The goal, stated plainly

Make the Valve Steam Frame track **controllers it was never sold with**. Not "enumerate",
not "show up in a menu" — produce usable pose and input for an arbitrary rigid body with LEDs
on it, eventually including Meta Quest 3 Touch Plus controllers and, farther out, an Index
controller wearing a custom emitter rig.

The attack order is dictated by what we can actually observe:

1. **Optical first.** IR LEDs are the one layer we can watch with our own cameras and model
   ourselves. The Frame controller carries 18 LEDs; Quest 3 Touch Plus carries 8.
2. **Inertial/radio later.** Both are locked behind closed protocols. We have no complete
   public reverse engineering of either link, so do not build a plan that assumes it.
3. **Present the result to SteamVR as a tracked device**, as a client of the running runtime.

The distinction that matters most: a controller appearing over radio is **not** the same as
the Frame tracking it optically. Say which one you mean, every time.

---

## 1. The frame of mind to arrive in

- **Evidence beats narrative.** Every factual claim carries a tag: `[V]` verified on our
  hardware (with date/build), `[D]` documented by Valve/Meta with a link, `[P]` press or
  community claim, `[I]` inferred by us, `[U]` untested question. If a note contradicts a doc,
  the note wins — but re-test before it becomes law.
- **Be a client, not a second tracker.** XRIPC is a singleton with one fixed set of
  `/dev/shm` mailboxes; the V4L2 tracking cameras are held exclusively by `XRService`
  while it runs. A second tracker is structurally impossible without breaking the first.
  Everything useful is reachable from outside through `libopenvr_api.so`, the dashboard,
  and `imageoverlay`. See `docs/08` — do not re-litigate it without new evidence.
- **Read-only first, reversible always.** Copy files off; do not edit them in place. Never
  patch a Valve binary. Never stop `XRService` or disable tracking for a casual test. If an
  experiment needs that, it needs an explicit yes and a rollback plan (EXP-007).
- **Small steps, written down.** Phases are gated: do not claim P1 until P0 has evidence in
  `notes/`. A negative result with a clear reason is a good result.
- **Assume the device changed.** This hardware is new and builds move. Record the build,
  date, and what you actually observed. A stale note is a clue, not a specification.
- **Privacy is part of correctness.** `captures/` and `local/` are git-ignored for a reason:
  room imagery, headset serials and the 340 MB filesystem mirror must never be committed.

---

## 2. Repository map — read in this order

| File | Why |
| --- | --- |
| `README.md` | Mission, evidence tags, quick commands, session convention |
| `ROADMAP.md` | Phases P0–P4, gates, parked ideas. **Check the current phase before starting work** |
| `AGENTS.md` | This file: operating instructions |
| `docs/07-ondevice-map.md` | **Read first** — verified process/driver/camera/settings map of the Frame |
| `docs/08-diagnostic-interface.md` | Why the answer is "OpenVR client, not second tracker"; build strategy and risk table |
| `docs/05-diagnostics-playbook.md` | What can be captured and how; IR rig, camera mirror, dataset recorder |
| `docs/11-getting-diagnostics-off-the-frame.md` | Copy-paste commands to pull logs/configs/datasets/cameras to the host |
| `docs/01`–`docs/04`, `docs/06` | Architecture, pairing, controller comparison, tracking limits, Quest 3 plan |
| `docs/09`, `docs/10` | Dataset decoding findings and the host viewer |
| `experiments/` | One file per experiment with hypothesis, procedure, result, confidence |
| `notes/YYYY-MM-DD-session.md` | Append-only session log. New day → new file; never rewrite history |
| `viewer/`, `tools/occblob.py`, `run.sh`, `CMakeLists.txt` | Host-side six-stream image viewer |
| `captures/`, `local/` | Git-ignored evidence and local device mirror. Never commit |

---

## 3. Host setup — install and run the viewer

This is the workstation side. This repo is its own Git root; run git commands here, not
from `~/workshop`. Its public remote is `origin` (`Frame-Contoler-Tracking`).

Prerequisites: CMake ≥ 3.16, a C++17 compiler, GTK 3 development headers, Python 3,
FFmpeg, and Git. The `gh` CLI is used for publishing but is not needed to build.

```bash
cd ~/workshop/SFControlerTracking
cmake -S . -B local/viewer-build
cmake --build local/viewer-build --parallel
```

The normal way to run it is the launcher, which also extracts the six streams from the
local recording into RAM-backed storage (takes a few minutes, substantial memory):

```bash
cd ~/workshop/SFControlerTracking
./run.sh
```

The input it expects is the verified recording at
`local/datasets/Deckard_2026-11-12_00-16-36.dataset/Deckard_2026-11-12_00-16-36.occ`.
For a different dataset, copy it in per `docs/11` and pass it through `tools/occblob.py`
yourself; `./run.sh --help` shows the direct-viewer modes (PNG folder or snapshot parent).

Viewer controls: buttons 1–6 switch cameras, **All 6** shows the grid (frame-aligned, not
timestamp-synced), **Space** play/pause, **Left/Right** step, **Home/End** jump, **F**
fullscreen, **Q** quit. Full details in `viewer/README.md`.

---

## 4. Frame setup — connect and collect

On the headset, once: **Steam Settings → System → Enable Developer Mode**, then
**Developer → Set User Password**. Keep the headset awake and on the same network. From
this workstation the SSH alias is `frame` (user `steamos`). If it does not resolve, add a
`Host frame` entry to `~/.ssh/config` with the headset's current address, or use the
SteamOS Devkit pairing flow (pairing requires **Settings → Developer → Pair new host** on
the headset). The `steamos-devkit` MCP can discover/pair/collect logs; its ADB target is
the **Lepton Android container**, not the native Linux shell — `adb shell` over USB-C
reaches native Linux. Do not conflate them.

Always start with the reachability check:

```bash
cd ~/workshop/SFControlerTracking
./scripts/frame-check.sh
```

Then follow `docs/11-getting-diagnostics-off-the-frame.md` for logs, OpenVR config,
the full read-only CV export, tracking datasets, headset-view frames and checksums.
Do not paste passwords, IPs or serials into committed files — use `frame` as the host name.

---

## 5. The real workbench — an OpenVR client on the Frame

The Frame already has a toolchain (g++ 15.1.1, cmake 3.29.2, python3 3.12.3), the OpenVR
headers at `/opt/steamvr/tools/hellovr_vulkan_linux/src/openvr/headers`, and
`libopenvr_api.so` at `/opt/steamvr/bin/linuxarm64/`. Build diagnostics **on-device**;
no cross-compilation, no PC in the loop.

Known-good live tools, run on the Frame:

```bash
cd /opt/steamvr/bin/linuxarm64 && LD_LIBRARY_PATH=. ./trackingtest   # live poses ~9 Hz
cd /opt/steamvr/bin/linuxarm64 && ./vrcmd --info                     # device inventory
cd /opt/steamvr/bin/linuxarm64 && ./vrcmd --pollcontrollers          # wait for controller events
cd /opt/steamvr/bin/linuxarm64 && ./vrcmd --dumpsensors              # sensor dump
```

The next piece of code to write is `sfct-diag`, a read-only OpenVR client (`VR_Init` with
`VRApplication_Background`, poses from `GetDeviceToAbsoluteTrackingPose`, CSV out). It is
**planned, not written** as of this note. Build pattern from EXP-006:

```bash
mkdir -p ~/sfct && cd ~/sfct
g++ -O2 -std=c++17 sfct_diag.cpp \
  -I/opt/steamvr/tools/hellovr_vulkan_linux/src/openvr/headers \
  -L/opt/steamvr/bin/linuxarm64 -lopenvr_api \
  -Wl,-rpath,/opt/steamvr/bin/linuxarm64 \
  -o sfct_diag
```

Risk ladder, cheapest first: OpenVR client, `imageoverlay`, dashboard developer toggles,
page injection — all safe and reversible. Writing `XRServiceSettings.json` is low-risk but
needs a restart and a backup. A second XRIPC session, stopping `XRService`, or patching a
Valve binary are high-risk and stay behind explicit permission.

For pushing annotated images into the headset today:

```bash
/opt/steamvr/tools/imageoverlay/bin/linuxarm64/imageoverlay /path/to/images/
```

---

## 6. Current phase and gates

- **P0 — Understand:** mostly done. Stack inventoried `[x]`, offline export verified `[x]`,
  diagnostic-interface answer decided `[x]`. Baseline controller behaviour and the numeric
  controller ceiling are still open.
- **P1 — See:** next. Build `sfct-diag` pose census, capture headset view, record a tracking
  dataset with camera video, stand up an independent IR rig, frame the LED constellation.
  **Gate:** a still frame where individual LEDs are visible and countable.
- **P2 — Detect:** our own blob detector + PnP pose solve for an LED rig.
  **Gate:** an LED-on-a-stick rig produces a stable, watchable pose.
- **P3 — Bridge:** Quest 3 controllers — radio recon, optical recon, reproduce the 8-LED
  constellation, input path, present as a tracked device.
- **P4 — Far goal:** Index controllers with an added emitter rig. The Index is a lighthouse
  *sensor*, not an emitter, and the Frame does not support base stations; this means building
  a visible rig and driving input separately, not "pairing an Index controller."

Do not start a later phase until the prior gate has evidence in `notes/`.

---

## 7. Working loop

1. Read the relevant doc and the latest `notes/` file before acting.
2. State the hypothesis and the evidence tag you expect, before running anything.
3. Capture into `captures/` (git-ignored); record date, build, what changed, result, tag.
4. Append a dated session entry. New day → new file. Never rewrite history.
5. Update the numbered `docs/` file when a fact changes, and the experiment file with the
   result. Mark old claims superseded rather than deleting them silently.
6. Commit small, descriptive changes and push to `origin/master`. Keep `captures/` and
   `local/` out of commits; check `git status` before `git add`.

---

## 8. Known walls — do not rediscover the hard way

- Raw tracking cameras (`/dev/video9`, `13`, `6`, `7`) are owned by `XRService`; streaming
  returns `Device or resource busy` while tracking runs. The headset-view mirror on
  `/dev/video99` is the composited view, not a raw tracking feed.
- XRIPC has twelve fixed shared-memory mailboxes, no per-client index, and an explicit
  `isThereASessionRunningInAnotherProcess()` guard. A second session corrupts the live one.
- `Show Debug Camera Views` is runtime-only: a dashboard click or page injection, not a
  persisted setting and not exposed through `vrcmd`.
- `vrcmd --startcapture/--stopcapture` are not the dataset recorder. That is UI-only.
- The full CV stack export is aarch64 binaries for reading, not for running.
- Lighthouse support and hand tracking on the Frame are unconfirmed/press-level; re-verify
  on the current build before spending money on hardware.

---

## 9. First-hour checklist

```bash
cd ~/workshop/SFControlerTracking
cat README.md ROADMAP.md AGENTS.md
cat docs/07-ondevice-map.md docs/08-diagnostic-interface.md
./scripts/frame-check.sh
git status -sb
```

Then: read the newest `notes/` file, check `docs/11` if you need to pull fresh diagnostics,
and pick the first unchecked item in the **current** phase of `ROADMAP.md`.

---

## 10. Sources

Valve's Steam Frame developer hub (setup, debugging, input, engines) and the public
hardware/CAD repository are the primary external references. Local field notes under
`~/.local/share/frame-control/docs/` are observations on particular builds, not immutable
specs. Every external claim in the docs carries a link or a tag; keep it that way.
