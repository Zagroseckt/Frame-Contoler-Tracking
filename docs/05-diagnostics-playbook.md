# 05 — Diagnostics playbook

Everything we can capture from the Frame, how to capture it, and what it tells us.

Ordered from cheapest/safest to most invasive.

---

## 0. Ground rules

- Don't cover the headset's cameras or the controllers' LEDs. Valve's support page explicitly
  blames coverings and modifications for tracking faults `[D]`. Our rigs must be removable.
- Record build/date with every capture. This hardware is new and behaviour will change.
- Never edit files on the Frame's read-only root that we can't restore. Prefer user paths.

---

## 1. Independent IR observation (no Valve software, no permissions)

**The cheapest instrument in the toolbox.**

Most phone cameras and many webcams see near-IR. Point one at a powered controller and the
LEDs show up as bright dots; the phone's own IR-cut filter may dim them, so try a front camera
and a rear camera, and try in a dark room.

Why this matters: it answers "is this controller emitting IR right now, and in what pattern?"
for controllers the Frame refuses to talk to. That is the single most important datum for the
Quest 3 plan.

What to record:

- video at the highest frame rate available (slow-mo mode if the phone has it)
- whether LEDs are **steady** or **strobing**, and if strobing, the pattern
- how many LEDs are visible from front / side / bottom
- behaviour in each state: off, powered but unpaired, pairing mode, paired to something else

Put frames in `captures/ir/` (git-ignored), conclusions in the experiment file.

---

## 2. Headset view mirror — `/dev/video99`

SteamVR runs `v4l2cam --output=99`, copying the headset's mirror (`system.HeadsetView`) into a
v4l2 loopback device `[V] 2026-09-26` (from `frame-control` field notes).

```bash
# On the Frame
ffmpeg -f v4l2 -i /dev/video99 -frames:v 1 /tmp/headsetview.png
# short clip
ffmpeg -f v4l2 -i /dev/video99 -t 5 -c:v libx264 -preset ultrafast /tmp/hv.mp4
```

Caveats already learned `[V]`:

- first frame read can be black — grab a few
- format is 1920×1080 RGB24
- hardware encoder (`/dev/video-enc0`) crashes `h264_v4l2m2m`; use `libx264` if encoding
- 720p30 ≈ 0.7 core, 1080p60 ≈ 1.7 of 8 cores

**This is the composited headset view, not a raw tracking camera.** It is useful for "what does
the wearer see" and for passthrough-ish frames. It is *not* a clean 4-camera tracking feed `[I]`.

**Verified 2026-11-11:** a capture while nobody was wearing the headset is a completely black
1920×1080 frame (6 KB PNG). Grab several frames and expect black when idle.

### Raw tracking cameras are real but locked

The tracking cameras are `GREY` V4L2 nodes (see `docs/07` §6):

- `/dev/video9`, `/dev/video13` — 1056×1024 (SLAM pair)
- `/dev/video6`, `/dev/video7` — 640×480 (upper pair)

Streaming them fails with `VIDIOC_REQBUFS ... Device or resource busy` while XRService runs
`[V]`. So there is no side-channel raw feed; getting one means stopping the tracking service.
Do not do that casually — it kills tracking and may upset the runtime.

---

## 3. Valve's tracking dataset recorder — the good stuff

Valve documents a dev feature that captures the entire tracking state over a window, with
**camera video, screen recording and audio as optional inclusions** `[D]`
([debugging doc](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging)).

Path: VR dashboard → Steam overlay → VR Settings → enable **Advanced Settings** → **Developer**
tab → **Record Tracking Data** → choose whether to include camera/screen/audio → START.

Valve's own framing `[D]`: it "logs a wide variety of sensors on the headset", and it exists so
they can replay tracking inputs. Ideal capture has tracking going from working to broken and
back.

Why we care: **a dataset with camera video enabled is the closest thing to a raw tracking-camera
feed we know of.** That is the "filters off" view, or at least the input to the filters.

Open questions `[U]`:

- where the dataset lands on disk (search for recently modified directories after a capture)
- whether camera video is all four cameras or just front
- whether it's raw or post-processed/downscaled
- retention/expiry behaviour

Capture procedure: `experiments/EXP-003-tracking-dataset.md`.

---

## 4. Looking for a marker/debug overlay

**Found 2026-11-11.** The dashboard already ships the switch:

- Label: **"Show Debug Camera Views"** (Settings → Developer tab,
  `Advanced_ShowDebugCameraViews`)
- Bound to `VRHTML.VRSystem.SetDebugCameraViewEnabled(bool)`
- Also in that tab: **Record Tracking Data** with **Allow Camera Recording**, and
  **Reset Controller Tracking Data**
- Separately: **Controller Tracking: Classic / Alternate** appears in controller settings

Catch: the toggle is **runtime-only**. It is not written to any settings file, and `vrcmd` has
no command for it (`--set-settings-bool` answers "Error setting"). Today it is either a
dashboard click or a page-injection job.

Fallback search strategy if the toggle disappears in a future build:

1. Developer settings tab — read every entry, screenshot with the headset view mirror.
2. `strings` over `driver_cv.so` / `XRService` for `DebugCameraView`, `displayOverlay`, `debugVis`.
3. SteamVR config (`steamvr.vrsettings`) for camera/debug keys.
4. Ask the runtime itself: `vrcmd --info`, `--dumpsensors`.

Keep expectations honest: the debug views may draw into the headset only, in which case
`vrcmd --screenshot` or the headset view mirror is how we get them onto a PC.

---

## 5. Logs

| Source | Where | Use |
|---|---|---|
| Steam logs | `cdl` shell helper on the Frame (per Valve doc) `[D]` | runtime events, device enumeration |
| SteamVR server log | usually `vrserver.txt` in the Steam log dir `[I]` | tracked device enumeration, driver messages |
| Lepton (Android) | `lepton dev logcat` `[D]` | only when an Android title/Lepton Development is running |
| systemd user units | `systemctl --user list-units` on the Frame `[I]` | which tracking services exist |

Grep targets once we have logs:

```bash
grep -iE "tracked|controller|pose|constellation|led|blob" vrserver.txt | head -100
```

---

## 6. Network / radio reconnaissance

- `bluetoothctl` scan on the Frame (Bluetooth 5.4 `[P]`) — does a Quest 3 controller in pairing
  mode advertise at all, and under what name?
- 2.4 GHz sniffing needs dedicated hardware (nRF52840-class sniffer, or an SDR). Not assumed
  available. `[U]`
- Frame's own controller link is proprietary `[I]`; do not expect it to appear as a standard
  HID device.

Safety: passive scans only. Do not attempt to impersonate or MITM a controller that is bound
to another headset — the goal is our own hardware.

---

## 7. Capture hygiene

- All captures go to `captures/` and are **git-ignored** (large binaries, personal environment).
- Every experiment records: date, build, what changed, result, confidence tag.
- Never commit anything containing credentials, IPs, or device serials.
