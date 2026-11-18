# SFControlerTracking

Research project: **how the Steam Frame tracks controllers, how controllers get paired to
the headset, and how far we can push that to non-Valve controllers.**

Started 2026-11-11. Owner: zagroseckt. Work area: `~/workshop/SFControlerTracking`.

This directory is its own Git root. Run git commands with this directory as the workdir,
not from `~/workshop`.

---

## Mission

Short version: make the Steam Frame accept controllers it was never sold with.

Long version, in increasing order of difficulty:

1. Understand the Frame's controller tracking pipeline end to end (optical + inertial + radio).
2. Document the pairing/binding path from the headset side.
3. Pair and track a set of **Meta Quest 3 (Touch Plus)** controllers on the Frame.
4. Eventually: mount tracking points on **Valve Index** controllers and get them recognised.

Step 4 is a different animal than step 3 and is parked under "Far goal" in `ROADMAP.md`.

---

## Today (2026-11-11) — definition of done

- [x] Work area created, named, initialised as its own Git repo.
- [x] Initial documentation set written (this file plus `docs/`).
- [x] Frame reachable on the network and inspected live.
- [x] Controller tracking stack inventoried on-device (binaries, configs, logs, services) — see `docs/07`.
- [x] Full tracking-system export pulled for offline examination — `local/frame-export/`, 70 files checksum-verified.
- [x] Answered: a separate diagnostic interface is possible as a *client*, not as a second tracker — see `docs/08`.
- [ ] Quest 3 controllers observed at the three layers we can actually observe:
      optical (IR LEDs), radio (2.4 GHz / BLE advertising), input (does anything appear as HID).
- [ ] First tracking-camera capture attempt (headset view + tracking dataset with camera data).

**Honest expectation for today:** full 6DoF Quest 3 tracking on the Frame is not a one-day
result. Quest 3 controllers talk a closed Meta radio protocol and emit a Meta LED
constellation that the Frame's vision system has no model for. What we *can* get today is
hard evidence: what the controllers emit, what the Frame sees, and exactly which of the
three layers has to be bridged first. That evidence is what makes the rest buildable.

---

## Reality check (read once, then get to work)

| Layer | What the Frame uses | What Quest 3 controllers offer | Bridgeable? |
|---|---|---|---|
| Optical | 6 outward cams (2 SLAM mono, 2 upper mono, 2 colour passthrough) + IR illuminators, looking for **18 IR LEDs** per Frame controller | **8 IR LEDs** (7 face + 1 handle), fixed constellation | Needs a custom constellation model. Hard but the most tractable layer. |
| Inertial | IMU in controller, fused with vision | IMU in controller, raw data only over Meta's radio | Blocked by radio layer. |
| Radio | Proprietary 2.4 GHz link, encrypted, headset-bound | Proprietary 2.4 GHz, encrypted, Meta-headset-bound | Currently no public complete reverse engineering. |
| Identity | Frame controller pairing flow | Meta pairing flow (phone app, Meta+B / Menu+Y) | Different binding roots. |

The order of attack is therefore: **optical first** (we can see IR with our own cameras),
radio later (needs dedicated sniffing hardware). Details in `docs/02` and `docs/06`.

---

## Map

| File | What's in it |
|---|---|
| `ROADMAP.md` | Phases, gates, what "done" means at each step |
| `docs/01-tracking-architecture.md` | How the Frame's tracking actually works, stage by stage |
| `docs/02-pairing-and-binding.md` | What "pairing" means at each layer, headset-side flow |
| `docs/03-controller-comparison.md` | Frame vs Quest 3 vs Index vs Quest Pro, side by side |
| `docs/04-tracking-limits.md` | **"How many controllers can it track?"** — the answer, and how to prove it |
| `docs/05-diagnostics-playbook.md` | Camera feeds, filters, markers, tracking datasets, log paths |
| `docs/06-quest3-controllers.md` | Quest 3 deep dive and the concrete attack plan |
| `docs/07-ondevice-map.md` | **Read this first** — verified map of the headset's tracking stack |
| `docs/08-diagnostic-interface.md` | Can we build a separate diagnostic interface? Yes as a client, no as a second tracker |
| `docs/09-plan-decode-and-viewer.md` | How the dataset streams were decoded and the viewer was built |
| `docs/10-occ-image-codec.md` | OCC stream extraction findings |
| `docs/11-getting-diagnostics-off-the-frame.md` | Copy-paste guide: logs, configs, datasets, camera frames and checksums off the headset |
| `experiments/` | One file per experiment, with pass/fail criteria |
| `notes/` | Session logs, dated, append-only |

---

## Evidence tags

Every claim in this project gets one of these, because the Frame is new and docs are thin:

- **[V]** verified on our hardware, with date and build
- **[D]** documented by Valve/Meta/official source, link included
- **[P]** press/community claim, not primary-sourced
- **[I]** inferred by us, not yet tested
- **[U]** untested / unknown — a question, not a fact

If a note contradicts a doc, the note wins, but it gets re-tested before it becomes law.

---

## Quick commands

To pull logs, configs, tracking datasets or camera frames off the headset, follow
`docs/11-getting-diagnostics-off-the-frame.md`. Start with `./scripts/frame-check.sh`.

For the host-side image viewer, run `./run.sh`. It prepares all six streams from the
verified local recording in temporary RAM-backed storage, then opens the viewer.
See `viewer/README.md` for controls, privacy notes, dependencies, and playback limits.

```bash
# Reachability check
ssh -o ConnectTimeout=5 frame 'uname -a; cat /etc/os-release | head -5'

# Headset view mirror (SteamVR publishes it to a v4l2 loopback device)
# On the Frame:
ffmpeg -f v4l2 -i /dev/video99 -frames:v 1 /tmp/headsetview.png

# Steam logs on the Frame
# cdl   (Shell alias: cd to Steam log directory)
# lepton dev logcat   (Android container logging, needs Lepton Development running)
```

A live capture helper lives at `scripts/frame-check.sh`.

---

## Session log convention

New session → new file `notes/YYYY-MM-DD-session.md`. Append, never rewrite history.
Record: what we tried, what the device said, what we concluded, what's next.
