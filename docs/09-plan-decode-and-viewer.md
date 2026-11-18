# 09 — Plan: decode the tracking dataset and build a viewer

Status: **historical proposal, approved and underway.** The implemented viewer uses GTK 3
and PNG sequences rather than the proposed SDL3 pack format. Its current features and
limitations are documented in `viewer/README.md`. The codec has been identified as H.264;
see `docs/10-occ-image-codec.md`. The remainder of this document preserves the earlier
proposal and should not be read as a description of completed features.
Predecessor: `docs/08-diagnostic-interface.md`. Related: `experiments/EXP-003-tracking-dataset.md`.

The goal of this step is narrow and concrete: turn the camera frames buried in the `.occ`
dataset into something we can *look at*, on a normal computer, with controls a non-technical
person can find without being taught.

---

## 0. Two questions settled before the work starts

**Colour passthrough cameras are not interfering. Do not remove them.** [V]

Nothing attempted so far has failed because of them. They are a separate pipeline from the
tracking cameras:

- Tracking / SLAM: `/dev/video9` + `/dev/video13`, 1056x1024, GREY (infrared). This is what
  the tracking system actually sees. [V]
- Colour passthrough: `/dev/video0` + `/dev/video3`, 2464x2464, NV12. This is what the
  wearer sees. [V]

Removing the colour cameras would sacrifice passthrough; the effects on room setup and
tracking have not been tested. There is no evidence removal would help. Leave them attached.

**Do not open a desktop session in the headset for this step.** [I]

The decode work happens on the host. A desktop is not needed; its effects on battery or
pose capture have not been measured. A
desktop may become useful in Phase 5, when running a viewer *inside* the Frame is on the
table. It is not a prerequisite.

---

## 1. Architecture

One pipeline, four pieces:

```
.occ on the Frame  ->  extractor (Python)  ->  frames.sfctpack  ->  viewer (C++/SDL3)  ->  run.sh
```

The important decision is that **the extractor and the viewer are separate processes with a
file boundary between them.**

Why:

- The codec work is the only genuinely unknown part. Keeping it in Python makes iteration
  cheap and keeps a possible dead end away from the display code.
- The viewer memory-maps a pack and needs no image library at playback time. Instant
  scrubbing, no decode cost, no directory with thousands of PNGs.
- If the codec turns out to be opaque, the extractor's *source* changes (raw v4l2 capture
  instead of `.occ`) and the viewer does not change at all. The demonstration survives.

### Pack format (`frames.sfctpack`)

- Small fixed header: magic, version, frame count, camera count.
- Index entries: timestamp, camera id, exposure, width, height, byte offset, pixel format.
- Payloads: raw GREY8 planes, 64-byte aligned.
- Little-endian throughout. Memory-mappable, so opening a multi-GB pack is instant.

---

## 2. Phase 0 — identify the image codec (delegated)

This is the one real unknown, so it gets a subagent with fresh context. The main session
stays lean.

**Job:** identify the image codec inside record types 32 and 38, field 13, and produce a
working reference decoder.

**Known-good inputs already established:** [V]

- Container is a tagged record stream: `"occipitl"` (8 bytes) + type (u32 LE) + length
  (u32 LE) + payload.
- Payload is TLV: field id (u32 LE) + size (u32 LE) + value.
- Type 32 carries SLAM frames, field 15 keys the camera (1, 2), field 13 holds the image
  blob (observed sizes 202968 / 198458 / 83276 / 88462).
- Type 38 carries the other four cameras, field 15 keys them (3, 4, 5, 6), field 13 blobs
  are small (2344 / 1778 / 659 / 2680).
- Type 32 field 11 is a constant 1.875. Field 33 is around 40.42 / 42.81 (unidentified,
  possibly exposure or gain).
- A raw 1056x1024 GREY frame is 1,081,344 bytes. A 202,968-byte blob is 5.33x compressed.
- Not JPEG, PNG, LZ4, zstd or Snappy by magic bytes.

**Leads, ranked by expected payoff:**

1. **Search for existing codec code on the device.** `libArcturusPerception.so` is unstripped
   with 109,767 symbols. An encoder may exist there; a usable decoder is not guaranteed.
   If we can call Valve's own decoder we skip bitstream reverse-engineering entirely. The
   Frame ships g++ 15.1.1 and cmake 3.29.2, so an on-device dumper is cheap.
2. **Battery of real decode attempts**, not just magic-byte checks. The 5.33x ratio suggests
   several possible schemes; the compression ratio alone cannot identify a codec.
3. **Structural analysis.** Byte-position entropy, periodicity, possible tile grid. Blob
   head `00 03 18 d4 65 b8 42 7f 7c a0 19 90 21 b5 5b 5a` admits several readings of the
   first two bytes (tile count, header length, dimension).

**Open question for the subagent:** field 13 can appear multiple times in one record. Are
those separate cameras, or pyramid levels of a single frame? This determines the pack's
indexing scheme.

**Deliverables:** `docs/10-occ-image-codec.md` (spec) and `tools/occblob.py` (dumps any blob
to an inspectable file).

**Constraints on the subagent:** read-only on the Frame, never commit `local/` or
`captures/`, write findings to files instead of narrating them.

While this runs, the full 495 MB dataset is pulled to the host in the background over an SSH
pipe, since `scp` fails against the IPv6 scope id.

---

## 3. Phase 1 — extractor (`tools/sfct-extract`)

Reads an `.occ`, walks records and TLV fields, decodes image blobs, writes `frames.sfctpack`.

Kept deliberately modest Python. No framework. One file, one job.

Supports extracting a time range or a camera subset, so we never have to materialise the
whole recording just to look at 30 seconds of it.

---

## 4. Phase 2 — viewer (`sfct-view`, C++17 + SDL3, CMake)

The requirement worth protecting: **control must be obvious to someone who was not told.**

Therefore:

- The full control list is drawn along the bottom of the window in large text.
- A help overlay is always one key away.
- `CONTROLS.txt` is shipped and `run.sh` prints the same list.
- Gamepad support, because friends reach for a controller before they read anything.

Keyboard:

- `Space` play / pause
- `Left` / `Right` step one frame
- `,` / `.` step ten frames
- `1`-`6` select camera, `0` quad grid of all cameras
- `G` cycle grid layouts
- `+` / `-` playback speed
- `B` brightness / gain (IR frames are dark; this matters more than expected)
- `C` colour map: greyscale, false-colour IR, heat
- `T` overlay text: frame index, timestamp, camera, exposure
- `P` save PNG into `captures/derived/`
- `H` help, `F` fullscreen, `Esc` / `Q` quit

Gamepad: A play/pause, B grid, shoulders step, left stick scrub, Y help.

Window title carries pack name and frame number.

Host already has everything needed, nothing to install: [V]

- SDL3 3.4.20, SDL2 2.32.74, SDL2_image 2.8.12, glfw3 3.5.1, Vulkan 1.4.363
- libpng 1.6.59, libjpeg 3.2.1
- GCC 16.2.1, CMake 4.4.5, Ninja 1.13.2
- Python 3.14.10, numpy 2.5.4, pillow 13.0.0, cv2 5.0.0

---

## 5. Phase 3 — one-click startup (`run.sh`)

No arguments required. Checks dependencies, configures and builds with Ninja if the binary
is stale, finds the newest pack in `captures/`, launches fullscreen.

Additional modes:

- `./run.sh --extract <dataset-dir>` build a pack from an `.occ`
- `./run.sh --list` show available packs

Errors must be readable sentences, not CMake output.

---

## 6. Phase 4 — reserve: direct raw capture

**Status: reserve only, per user. Ask for explicit permission at the time if it becomes
useful; do not proceed based on this plan alone.**

Outline: stop `XRService`, raise `/sys/class/leds/mp3317-scene-illum-ir/brightness`, capture
from `/dev/video9` and `/dev/video13` via v4l2, restart `XRService`. Tracking is dead for
for an unknown duration; successful capture and recovery are not guaranteed.

Value is limited and understood: it shows what the sensors physically see, which is useful
for judging whether a foreign controller's IR LEDs are visible. It does not carry tracking
metadata, poses, or timestamps tied to the tracking pipeline. It is a sensor-visibility
diagnostic, not a data source.

---

## 7. Phase 5 — showing off

Two venues, both later:

- This rig's monitor, for people standing around.
- Inside the Frame via `imageoverlay` (`Usage: imageoverlay <list of image or image
  directories>`), which floats supplied images in VR. Requires decoded frames to exist
  first, so it strictly follows Phase 1.

---

## 8. Risks

**Codec is a hardware-accelerated custom bitstream.** Most likely risk. Mitigation: on-device
decoder dumper (Phase 0 lead 1) produces PNGs instead of bitstreams. Slower pipeline, same
demonstration.

**Blobs are not images at all.** Less likely. The compression ratio and per-camera keying
argue against it.

**Nothing here changes the Frame.** All capture is read-only, and the only Frame-side build
is a small probe binary in `~/sfct/`.

---

## 9. Approval needed

1. Plan approved, or amended.
2. Headset kept awake for roughly ten more minutes to pull the dataset.
3. Raw capture: reserve only, ask at the time. Confirmed by user.
4. Desktop in headset: not needed for this stage; user confirmation not required.
