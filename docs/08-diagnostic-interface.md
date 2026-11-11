# 08 — Can we build a separate diagnostic interface?

Short answer: **yes, as a client — no, as a second tracker.**

That distinction is the whole game, so this doc nails it down with evidence from the files we
pulled off the headset. Reference material: `local/frame-export/` (340 MB, 70 files,
`MANIFEST.sha256` verified, 0 mismatches). Everything below is offline analysis of those copies.

---

## 1. The three layers you could "instance"

The Frame's tracking stack is not one blob. It is three separable layers, and each has a
different answer to "can I run my own?"

| Layer | What it is | Can we run our own? |
|---|---|---|
| **OpenVR client** | anything that talks to `vrserver` through `libopenvr_api.so` | **Yes.** Fully supported. This is what `trackingtest` and every VR app do. |
| **XRIPC session** | the private Channel between `driver_cv.so` and `XRService` | **No.** Singleton shared memory, one mailbox set. |
| **V4L2 camera** | raw `/dev/video*` tracking sensors | **No.** Exclusively held by `XRService` while it runs. |

The good news: **layer 1 is the one you actually want**, and it costs nothing in risk.

---

## 2. Layer 1 — OpenVR client: safe, supported, already proven

`vrserver` is the hub. It enumerates devices, owns the tracking poses, and answers any client
that attaches through `libopenvr_api.so`. A client never touches cameras, never touches
`XRService`, never touches the radio. It asks "where is device N?" and gets an answer.

**It already works on this hardware.** `trackingtest` is a stock OpenVR client:

```bash
cd /opt/steamvr/bin/linuxarm64 && LD_LIBRARY_PATH=. ./trackingtest
```

It printed live HMD poses at roughly 9 Hz on 2026-11-11 while the normal SteamVR stack was
running, unmodified. That is the proof of concept, sitting on the device, free `[V]`.

### What a diagnostic client can read

Through `IVRSystem::GetDeviceToAbsoluteTrackingPose`, a client gets an array covering every
tracked device index, each with:

- `mDeviceToAbsoluteTracking` — full 3×4 pose
- `bPoseIsValid`, `bDeviceIsConnected`
- velocity and angular velocity
- `eTrackingResult` — the tracking-quality enum (`Running_OK`, `Running_OutOfRange`,
  `Fallback_RotationOnly`, etc.)

Per device, `IVRSystem` also exposes `GetTrackedDeviceClass`, `GetTrackedDeviceIndexForControllerRole`,
`GetControllerStateWithPose`, `GetStringTrackedDeviceProperty` (serial, model number, render
model name, battery), and `GetControllerAxisState`.

**That is enough to answer every question on your list without a single invasive move:**
how many controllers the runtime admits to having, what each one's pose is moment to moment,
and whether a third emitter ever appears as a device.

### And the toolchain is already on the Frame `[V]`

```
gcc/g++ 15.1.1 · cmake 3.29.2 · make 4.4.1 · python3 3.12.3
/opt/steamvr/tools/hellovr_vulkan_linux/src/openvr/headers/openvr.h      (281 KB)
/opt/steamvr/bin/linuxarm64/libopenvr_api.so   → VR_InitInternal, VR_GetGenericInterface
```

So we do not cross-compile and we do not need a PC in the loop. A small C++ client builds
on-device with `g++ main.cpp -I/opt/steamvr/tools/hellovr_vulkan_linux/src/openvr/headers
-L/opt/steamvr/bin/linuxarm64 -lopenvr_api -Wl,-rpath,/opt/steamvr/bin/linuxarm64`.

### What it costs

Nothing. Attaching a read-only client to `vrserver` is exactly what Steam itself does for the
desktop mirror and for `vrcmd`. No camera contention, no session conflict, no restart. If our
client crashes, SteamVR does not notice.

---

## 3. Layer 2 — XRIPC: the wall, and why it is a wall

`libArcturusPerception.so` is unstripped — **109,767 defined symbols, 5,790 dynamic** `[V]`. That
means we can read its entire interface, and the interface is genuinely tempting:

```
PE::XRIPCSession::startSession(char const*)
PE::XRIPCSession::setDynamicOptions(char const*)
PE::XRIPCSession::setDatasetRecordingEnabled(bool, bool, bool, bool)
PE::XRIPCSession::getTrackingMetrics(PE::TrackingMetrics&)
PE::XRIPCSession::pauseTracking() / resumeTracking() / resetTracking()
PE::XRIPCSession::isThereASessionRunningInAnotherProcess()
PE::XRIPCSession::isThereASessionRunningInAnyProcess()
PE::XRIPCSession::storeRoyDongleSerial(char const*, bool)
```

and a full client call table: `call_ReadWorldPoses`, `call_PredictWorldPose`,
`call_UltraLowLatencyPredictWorldPose`, `call_GetControllerTrackingMetrics`, `call_GetSparseMap`,
`call_GetRoomLayout`, `call_GetLastSegmentationResult`, `call_GetPlaneAnchorSamples`,
`call_SetNumericalProperty`, `call_SetTrackerAndPreprocessorPaused`, and the chaperone set.

Plus server→client callbacks, which is where the injection points live:

```
callClient_UploadControllerConfig(char const*, char const*, unsigned int, bool)
callClient_NotifyControllerLedBrightness(char const*, double, unsigned int, bool)
callClient_SessionSettingsChanged
callClient_SetRoomViewType / SetRoomViewEnabledForConstruct / SetRoomViewWireframeEnabled
callClient_SetTemporarilyUseLHInsteadOfSLAMPoses
callClient_DatasetRecordingStarted / DatasetRecordingStopped
```

`setDatasetRecordingEnabled(bool, bool, bool, bool)` — four flags. That is almost certainly
enabled/camera/screen/audio, matching the UI's four checkboxes. So the dataset recorder *is*
reachable programmatically. Just not by us.

### Why not: the mailbox is a singleton `[V]`

`XRService` implements a fixed set of POSIX shared-memory mailboxes, visible in `/dev/shm`
while it runs. Exactly these twelve, no index, no per-client suffix:

```
XR_ClientRequest_VeryLow_Data     XR_ServerResponse_VeryLow_Data
XR_ClientRequest_Low_Data         XR_ServerResponse_Low_Data
XR_ClientRequest_Mid_Data         XR_ServerResponse_Mid_Data
XR_ClientRequest_High_Data        XR_ServerResponse_High_Data
XR_ClientResponse_Low_Data        XR_ServerRequest_Low_Data
XR_ClientResponse_VLow_Data       XR_ServerRequest_VLow_Data
```

Sizes: VeryLow 1024 B, Low 4 KB, Mid 2.9 MB, High 10 MB. Four latency classes, one pair of
channels per class. Both `vrserver` (pid 2199) and `XRService` (pid 2300) hold all twelve open.

The symbol table confirms the shape: `XRIPCPeer::clientFunctionCallLock_veryLowLatency()`,
`_lowLatency()`, `_midLatency()`, `_highLatency()` — one mutex per latency class, not per client.
And the diagnostic exists for exactly this reason:
`isThereASessionRunningInAnotherProcess()`. The API asks that question because the answer is
supposed to be "no".

Consequence: there is no second mailbox set to open. A second process wanting to "instance the
tracking system" has three bad options — write into the same request buffer (races
`driver_cv.so` and corrupts a live session), open the cameras (already held), or start its own
`XRService` (needs the cameras). All three end in broken tracking.

**If you ever want to probe this anyway**, it is a *stopped-`SteamVR* experiment with a rollback
plan, not a live one. Filed as EXP-007, gated behind explicit permission.

### One honest caveat

The `/dev/shm/XR_*` files are mode `0666`, so they are technically readable by any local
process. Passive observation of the response buffers is *conceivable* — but they are
double-buffered with sequence numbers and consumed on read, so "just snoop it" is a research
project with a real chance of stepping on the live client. Not a foundation for a diagnostic
tool, and not necessary either, given layer 1.

---

## 4. Layer 3 — driving the existing UI: safe, and it unlocks the good stuff

Some of the best diagnostics are not readable through OpenVR; they live inside the dashboard.
But the dashboard runs *in* the existing stack, so commanding it is not a second instance —
it is the same instance being told to do something.

Already shipped and confirmed present `[V]`:

| Control | Effect | Access |
|---|---|---|
| **Show Debug Camera Views** (`Advanced_ShowDebugCameraViews`) | camera feed with tracking overlay in-headset | `VRHTML.VRSystem.SetDebugCameraViewEnabled(bool)` — dashboard click only today |
| **Record Tracking Data** (`Advanced_RecordTrackingData`) | dataset recording incl. camera frames | dashboard; `allowTrackingCameraRecording` already `true` in config |
| **Controller Tracking: Classic / Alternate** | selects the tracking pipeline | controller settings page |
| **Reset Controller Tracking Data** | clears controller tracking state | controller settings page |

The camera-view toggle is runtime-only: not persisted, not scriptable via `vrcmd`
(`--set-settings-bool` answers "Error setting" for arbitrary keys). Two ways in, both
non-invasive:

1. **Click it.** Settings → Developer → Show Debug Camera Views.
2. **Page injection.** Turn on `Enable web helper debugger`, attach Chromium devtools to the
   dashboard page, and call `VRHTML.VRSystem.SetDebugCameraViewEnabled(true)` from the console.
   Same process, same permission, no restart.

### A ready-made overlay already exists `[V]`

`/opt/steamvr/tools/imageoverlay/bin/linuxarm64/imageoverlay` takes a list of images or
directories:

```
Usage: imageoverlay <list of image or image directories>
Advance Image: click on right side
Decrement Image: click on left side
Exit: click off the image
```

So we can push arbitrary PNGs into the headset view *today* — annotated constellation diagrams,
tracking-marker mockups, screenshots, whatever. That is a diagnostic display surface that costs
us no code at all.

---

## 5. So what does the diagnostic interface look like?

**Name:** `sfct-diag` (Steam Frame Controller Tracking — diagnostic). Lives in `tools/`, built
on-device, git-ignored binaries.

### Milestone 1 — pose census (does not exist yet, effort: small)

A C++ OpenVR client that every 100 ms prints one line per tracked device index:

```
idx  class      serial        valid  result              pos (m)              rot (quat)
1    HMD        FPRAH62100F49  Y     Running_OK          -0.36 -0.15 0.62     0.02 -0.71 0.00 0.71
2    Controller 6cb1df7a24e1   Y     Running_OK          ...                  ...
3    Controller ebc3df5704f3   Y     Running_OK          ...                  ...
```

Answers "how many controllers can be tracked" with hard numbers, and simultaneously becomes the
ground-truth reference for any optical pipeline we write later. This is the single highest-value
piece of code in the project.

### Milestone 2 — live map view (effort: medium)

Same client, plus a top-down 2D plot of every device's position over time, and per-device
tracking-quality state colouring. Good for spotting a controller dropping to rotation-only, i.e.
the moment the LED constellation stops being seen.

### Milestone 3 — in-headset frames (effort: small, no code)

`imageoverlay` pointed at a directory we drop PNGs into. Camera frames if the debug view gives
us any, plus our own annotated diagrams.

### Milestone 4 — recording (effort: medium)

Log every pose to CSV at full rate. That is the measurement backbone: with a CSV we can compare
"what the Frame thinks" against "what our own optical solver thinks", controller by controller.

### Explicitly out of scope

- A second `XRService` instance. Blocked structurally (§3).
- Direct camera capture while tracking runs. Blocked by the driver (§ `docs/07` §6).
- Anything that requires patching a Valve binary.

---

## 6. Risk table

| Move | Risk to normal Steam use | Reversible? |
|---|---|---|
| OpenVR client attaching to `vrserver` | none — this is the supported path | trivially |
| `imageoverlay` running | none | yes |
| Dashboard developer toggles | none; runtime state only | yes |
| Page injection via web helper debugger | none; it is a documented debug switch | yes |
| Writing `XRServiceSettings.json` | **low**; needs restart, malformed values are logged and ignored | back up first |
| Second XRIPC session | **high** — corrupts live mailboxes | restart required |
| Stopping/restarting `XRService` | **medium** — tracking drops, cameras re-init | restart |

The first four are free. The fifth is a planned experiment with a backup. The last two stay
behind an explicit yes from you.

---

## 7. The offline export

`local/frame-export/` mirrors the device's absolute paths, so a path in any doc is a path in the
mirror:

```
local/frame-export/opt/steamvr/drivers/cv/...
local/frame-export/opt/steamvr/bin/linuxarm64/{trackingtest,vrcmd,libopenvr_api.so,...}
local/frame-export/home/steamos/.config/openvr/...
local/frame-export/home/steamos/.local/share/Steam/logs/...
local/frame-export/MANIFEST.sha256
```

Verified with `sha256sum -c MANIFEST.sha256` → 70 files, 0 mismatches `[V]`. Nothing on the
Frame was modified; the whole pull was read-only.

### What the copies are good for

- **Reading symbols** — `libArcturusPerception.so` is unstripped, so
  `nm -C --defined-only` yields the entire internal API, including `PE::XRIPC*`, `PE::PoseSolver`,
  `PE::PosePredictor`, `PE::XRPose`, `ST::InfraredFrame`, `ST::KBSplineIntrinsics`.
- **Diffing across updates** — when Valve ships a new build, `sha256sum` + `diff` tells us exactly
  what changed in the tracking stack.
- **Searching safely** — `grep`/`strings` without a network hop or a running headset.
- **Not** for running. These are aarch64 binaries; and running an exported copy of `XRService`
  is precisely the thing we just established not to do.

`XRService` itself is stripped enough to be unhelpful at the symbol level (it is an executable,
and our copy has no `.dbg`), so the useful symbol archaeology is all in
`libArcturusPerception.so` and `driver_cv.so` (3,422 dynamic symbols).

---

## 8. Answer, in one paragraph

You can absolutely build your own diagnostic interface, and it will not disturb normal Steam
use — **but it has to be a client of the running runtime, not a fork of it.** The tracker is
built around a single-occupancy shared-memory mailbox and it holds the camera nodes exclusively,
so a second instance is structurally impossible without breaking the first. Everything you
actually asked for — how many controllers, where they are, what the tracking quality is, the
camera views, the marker overlay, the recording — is reachable from outside: poses through
`libopenvr_api.so`, images through `imageoverlay`, camera views and dataset recording through
dashboard commands. The build itself is a modest C++ client against headers that are already on
the device.
