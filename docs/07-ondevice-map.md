# 07 — On-device map (verified 2026-11-11)

Everything here was read off the headset today. Build: SteamVR `2026-09-23 23:30:05`,
SteamOS on aarch64, HMD serial `FPRAH62100F49`, `model_number: "Deckard MP"`, revision 6.

This is the map we were missing. Read it before touching anything.

---

## 1. The process stack

Running while the headset is awake:

| Process | Path | Role |
|---|---|---|
| `vrserver` | `/opt/steamvr/bin/linuxarm64/` | runtime core, device enumeration |
| `XRService` | `/opt/steamvr/drivers/cv/bin/linuxarm64/` | **the tracking engine** |
| `dsp_service` | same | DSP/sensor offload |
| `v4l2cam --output=99` | `/opt/steamvr/bin/linuxarm64/` | headset view → `/dev/video99` |
| `vrcompositor`, `vrdashboard`, `vrmonitor` | | presentation, UI |
| `eyetracking` | `/opt/steamvr/tools/eyetracking/bin/linuxarm64/` | eye tracking with a `.weights` model |
| `vrwebhelper` ×N | | the Chromium-based dashboard UI |

Extra binaries worth knowing: **`trackingtest`** (live pose printer), **`vrcmd`** (runtime
query/control CLI), `openvr_api_canary`, `overlay_viewer`.

## 2. Driver layout

`/opt/steamvr/drivers/`: `cv`, `deckard`, `frame_controller`, `frame_hmd`, `htc`,
`indexcontroller`, `indexhmd`, `oculus`, `prism`.

Notes:

- `cv` is the only real tracking driver here (`hmd_presence: 28DE.2300`).
- `frame_controller`, `frame_hmd`, `htc`, `indexcontroller`, `indexhmd` are **`resourceOnly: true`**
  — they ship render models/bindings, not device drivers. So `indexcontroller` present on disk
  does *not* mean the Frame can talk to Index controllers. It's cosmetic resources. `[V]`
- `prism` handles display redirection.

## 3. The computer-vision stack

`/opt/steamvr/drivers/cv/bin/linuxarm64/`:

- `XRService` — the tracking application itself
- `libArcturusPerception.so` — Arcturus-supplied perception library (confirms the press claim
  that Arcturus built the Frame's camera tracking)
- `libonnxruntime_linux_arm64.so`, `libmediapipe_linux_arm64.so` — NN inference, MediaPipe
- `driver_cv.so` — the OpenVR driver shim that spawns/controls XRService
- `CAMERA_ICP.mbn`, `adsp_sm8650.mbn`, `cdsp_sm8650.mbn`, `evass.mbn` — Qualcomm DSP firmware
- `xrservice_third_party_legal_notices.txt` — worth a read for component list

### Models in `resources/`

- `dipr_models/` — **inertial** pose regression (DIPr), not optical:
  - `ev1/` — currently loaded for our HMD (`DIPr: using per-device model .../ev1/...` in the log)
  - `index/` — Index HMD inertial model
  - `dipr_v2_controller/`, `dipr_v3_controller/` — controller inertial models
- `minControllerFeatureNNModel/` — controller **feature** extraction NNs, with
  `v2Right.json`, `v4Right.json`, `k2v3Right.json` configs and `CONTROLLER_PALM_MIDDLE` /
  `CONTROLLER_CENTERED` bounding-box definitions
- `arcfeat_models/`, `ep2_models/`, `segm_models/` — feature, eye/pose and segmentation NNs
- `qrcode_*` — QR detection (room setup / pairing codes?)
- `devignette_*.dds` — camera vignetting correction maps
- `shaders/debayer_*` — debayer compute shaders (proves the colour sensors feed this pipeline)

## 4. The controller constellation — the real prize

Per-controller calibration files live in **`/home/steamos/.config/openvr/config/roy/`**, named
by controller serial:

- `6cb1df7a24e1.json` → `model_number: "Roy_Right"`, role `right_hand`
- `ebc3df5704f3.json` → `model_number: "Roy_Left"`, role `left_hand`

Each contains `lighthouse_config.modelPoints` + `modelNormals`, plus IMU extrinsics
(`imu.position`, `plus_x`, `plus_z`). (Yes, "lighthouse_config" is the legacy key name for the
optical point model — ignore the misleading name.)

Measured from those files:

| Property | Value |
|---|---|
| LED model points | **18 per controller** |
| Normals | 18 (one per point, outward direction) |
| Bounding box | ≈ 64 mm (x) × 114 mm (y) × 66 mm (z) |
| Closest LED pair | **12.3 mm** |
| Points on lower handle | 3 |
| Mirror symmetry | left/right are exact mirror images in x |

A second variant exists in the shipped resources:
`minControllerFeatureNNModel/k2v3Right.json` → `model_number: "Roy_K2V3_Right_CV"` with
**17 points** and a `channelMap` of 17 entries. So there is at least one other controller
revision with a different emitter count. `[V]`

**Why this matters:** this file *is* the optical identity of a controller. Pairing creates it,
the vision system consumes it. It is the most plausible injection point we have for teaching the
headset about a new rigid body — see `docs/02`.

## 5. Pairing, as the binary describes it

`driver_cv.so` contains a `CRoyPairing` subsystem with these log strings:

- `CRoyPairing: discovered new %s OOBE controller: %s`
- `CRoyPairing: attempting to connect to Left/Right OOBE controller candidate: %s`
- `CRoyPairing: new bootstrap: %s (%d)`
- `CRoyPairing: pairing to OOBE controller: %s`
- `CRoyPairing pairing requested for %d seconds` / `pairing stopped` / `pairing timed out`
- `CRoyPairing identify controller '%s'`, `disconnect controller`, `unpair controller`
- `Controller %s: Paired`

Read together with the `roy/<serial>.json` files, the flow is:

1. Radio-level discovery of an unpaired ("OOBE") controller.
2. A timed pairing window where a bootstrap exchange happens.
3. On success, the controller is identified by serial and a model file is written for it.
4. From then on, optical detection + radio identity are married.

That is a **radio-first** pairing. There is no evidence of a vendor-agnostic path.

## 6. Cameras — six outward sensors, not four

The XRService log names them:

| Name | Sensor | I²C | CSIPHY | Subdev |
|---|---|---|---|---|
| `passthrough_left` | `arcimx616` (Sony IMX616, colour) | 0-001a | csiphy3 | v4l-subdev29 |
| `passthrough_right` | `arcimx616` | 0-0010 | csiphy5 | v4l-subdev28 |
| `slam_left` | `og01a1b` (OmniVision, mono) | 4-0060 | csiphy0 | v4l-subdev30 |
| `slam_right` | `og01a1b` | 4-0036 | csiphy1 | v4l-subdev31 |
| `upper_left` | `og0ve10` (OmniVision, mono) | 5-0060 | csiphy0 | v4l-subdev32 |
| `upper_right` | `og0ve10` | 5-003e | csiphy1 | v4l-subdev33 |

`Cached 6 camera paths`. Eye cameras are handled separately by the `eyetracking` tool.

Press said "four monochrome cameras". The truth is **two colour-capable passthrough sensors +
two SLAM mono + two upper mono**. Two of the six are the passthrough pair. `[V]`

### V4L2 node fingerprint

Formats were read without streaming (safe):

- `/dev/video0`, `/dev/video3` — 2464×2464 NV12 → passthrough (IMX616)
- `/dev/video6`, `/dev/video7` — **640×480 GREY** → upper cameras
- `/dev/video9`, `/dev/video13` — **1056×1024 GREY** → SLAM cameras
- the rest are 1920×1080 UYVY encoder/ISP nodes

### They are exclusively held

```
v4l2-ctl -d /dev/video9 --stream-mmap --stream-count=1
→ VIDIOC_REQBUFS returned -1 (Device or resource busy)
```

Same for 6, 7, 13. XRService owns them. **You cannot capture tracking cameras while tracking
runs.** Direct capture would require stopping XRService — i.e. killing tracking. Not a casual
diagnostic; treat as a last resort. `[V]`

### Calibration data in the log

Tracking cameras: 1056×1024, `KBSpline` fisheye model, fx≈328, fy≈328, cx≈526, cy≈513, with
spline coefficients printed in full. Passthrough: 2464×2464, `KBPolarSpline`, fx≈1476.

Also logged: `ISP disabled for tracking cameras (passthrough using main VFE)`, `Upper cameras
FPGA interleaving support: 1`, and EEPROM calibration ingestion for passthrough.

## 7. IR illuminator

Sysfs device: `/sys/class/leds/mp3317-scene-illum-ir/`

- has `Channel left`, `Channel right`, `brightness` (currently 0), `max_brightness` 4095,
  `led_fault`, `led_ovp_threshold`, `led_state`
- the log records `[IREmitters] IR emitters mode changed to Auto`

So illumination is software-controlled with a manual brightness knob. Useful for contrast
experiments later. Do not blind yourself; IR is invisible and painless.

## 8. Settings surface (the most useful lever)

Live settings: `/home/steamos/.config/openvr/config/cv/xrservice/XRServiceSettings.json`

Current contents are almost empty:

```json
{ "xrSessionSettings": { "lightSourceFrequencyEstimate": 60 } }
```

But the binary knows a much bigger schema, including migration messages that reveal real paths:

- `captureSessionSettings/trackingCameras/useAutoExposure`
- `captureSessionSettings/trackingCameras/useDynamicGamma`
- `captureSessionSettings/passthroughCameras/useAutoExposure`
- and the top-level sections `xrSessionSettings`, `minControllerSettings`,
  `captureSessionSettings`, `cropSettings`, `readPrioritySettings`

Unknown/mistyped keys are reported: `XRServiceSettings/defaultSettings type mismatch for key
(%s). Expected type: %s, got type: %s.` There is also a `strictXRServiceSettingsCheck`.

### Option names worth our time

Extracted 9,150 candidate field names from `XRService`
(`captures/logs/xrservice-option-names.txt`). Highlights:

| Option | Why it matters |
|---|---|
| `enableFewLedTrackingMode` | a mode for sparse LED sets — exactly the Quest 3 situation |
| `estimateYawFor1LEDControllers` | code path for 1-LED controllers; the tracker is generalised beyond its own hardware |
| `filterTrackedLedsForOtherControllers` | multi-controller LED attribution exists |
| `alternateNNTrackingBetweenControllers` | NN tracking scheduled across controllers |
| `fallbackToPoseResidualIfNotAllLedsAreVisbled` | partial-visibility handling |
| `disableLedBootstrapping`, `disableLedTrackingInView` | ways to turn bits off for experiments |
| `controllerLEDsOffWhenSLAMDisabled` | emitter behaviour coupling |
| `TrackAllCameras` / `TrackMainStereoCamerasWithFallbackToSecondaryCameras` | camera participation policy |
| `debugVis`, `debugOutput`, `detectMarkers`, `displayOverlay` | possible debug visualisation switches |
| `analyzeFrameExposureAndAdjustCameraParameters` | exposure control |
| `controllerMccfMaxRecordingFps` | recording path |

`controllerFeatures`, `controllerFeaturesPalm`, `controllerTrackingOptions`,
`minControllerSettings` are the controller-specific groups.

**Untested:** we have not yet tried writing any of these, because XRService reads settings at
startup and a restart is disruptive. That's the next controlled experiment.

## 9. UI toggles that already exist

From `/opt/steamvr/resources/webinterface/dashboard/localization/vrmonitor_english.json`:

- **`Advanced_ShowDebugCameraViews` — "Show Debug Camera Views"** (Developer tab). Wired to
  `VRHTML.VRSystem.SetDebugCameraViewEnabled(bool)`. This is the "see what the cameras see"
  switch.
- **`Advanced_RecordTrackingData` — "Record Tracking Data"** with **Allow Camera Recording**,
  Allow Screen Recording, Allow Audio Recording. Settings already enabled in
  `steamvr.vrsettings`: `allowTrackingCameraRecording: true`.
- **`Settings_Controller_ResetTracking` — "Reset Controller Tracking Data"**
- **`Settings_ControllerTracking_Header` — "Controller Tracking"** with options
  **Classic** and **Alternate**

The camera view toggle is **runtime-only** (JS default `false`, not persisted to a settings
file). It is not exposed through `vrcmd`; `--set-settings-bool` returns "Error setting" for
arbitrary keys. So today it is a dashboard click, or a page-injection job.

## 10. Tooling cheat sheet

```bash
# live headset pose at ~9 Hz, works today
cd /opt/steamvr/bin/linuxarm64 && LD_LIBRARY_PATH=. ./trackingtest

# device inventory
./vrcmd --info

# wait for controller poses (blocks until events arrive)
./vrcmd --pollcontrollers

# sensor dump (currently only lists the HMD)
./vrcmd --dumpsensors
```

`vrcmd` also has `--dump-positions`, `--pollposes`, `--watchdog`, `--showmodels`,
`--screenshot`, `--send-vrevent`. `--startcapture/--stopcapture` are **not** the tracking
dataset recorder (both answered `unknown command`), so that stays a UI operation.

## 11. The offline export (2026-11-11)

The whole tracking stack was copied off the headset so the originals are never touched:

```
local/frame-export/   ~340 MB, 70 files, MANIFEST.sha256 verified (0 mismatches)
```

Layout mirrors the device's absolute paths. Included: all of `/opt/steamvr/drivers/cv`
(binaries, models, shaders, resources), `trackingtest` + `vrcmd` + `libopenvr_api.so`, the full
`~/.config/openvr/config/` tree (including `roy/` and `xrservice/`), and the day's logs.

Offline findings:

- `libArcturusPerception.so` is **unstripped** — 109,767 defined symbols, 5,790 dynamic. The
  entire perception-engine API is readable (`PE::XRIPC*`, `PE::PoseSolver`, `PE::PosePredictor`,
  `ST::InfraredFrame`, `ST::KBSplineIntrinsics`).
- `driver_cv.so` has 3,422 dynamic symbols; `XRService` is effectively stripped and ships no
  `.dbg` anywhere on the device.
- The XRIPC layer is a **single-client** design: twelve fixed shared-memory mailboxes in
  `/dev/shm` (`XR_ClientRequest_{VeryLow,Low,Mid,High}_Data` plus responses) with no per-client
  index, one mutex per latency class, and an explicit
  `isThereASessionRunningInAnotherProcess()` guard.

That last point is the reason a second tracking instance is impossible, and why the diagnostic
interface has to be a client. Full argument in `docs/08-diagnostic-interface.md`.

## 12. What is still blocked

1. `Show Debug Camera Views` — needs a dashboard click or page injection.
2. Tracking dataset — UI-only, and we haven't captured one yet.
3. Any settings experiment — needs an XRService restart, i.e. a SteamVR restart.
4. Controllers were asleep during this session, so no live controller pose data yet.

A separate diagnostic interface is **not** on this list: it is buildable today as an OpenVR
client. See `docs/08-diagnostic-interface.md`.

---

Tags: every statement above is `[V]` unless marked otherwise.
