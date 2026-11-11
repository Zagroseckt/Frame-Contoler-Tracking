# 04 — How many controllers can be tracked?

This is the first question asked, so it gets its own file. Short answer, long answer, then how
to actually prove it.

## Short answer

**More than two is plausible. The exact ceiling on the Frame is unknown and unverified.**

Known bounds from adjacent systems:

| System | Bound | Source |
|---|---|---|
| OpenVR device slots | **64** tracked devices (`k_unMaxTrackedDeviceCount`) | `openvr.h` `[D]` |
| SteamVR + Lighthouse, practical | 11 trackers, or 13 without controllers, inside a 16-device budget | HTC Vive Tracker dev guidelines `[D]` |
| SteamVR + Lighthouse, updated enum | up to 64 devices after OpenVR update; Unity plugin historically capped at 16 | Valve/HTC docs + community `[D]/[P]` |
| Base stations per space | 4 (Lighthouse 2.0, up to 16 channels to avoid interference) | Valve/HTC `[D]` |
| Meta Quest 3 | 2 controllers (hand tracking is a separate path) | Meta `[D]` |
| Steam Frame | **not published** `[U]` | — |

## Why the Frame's number is different from SteamVR's 64

The 64-device number is a *host-side enum limit* in OpenVR. It says nothing about how many
objects the Frame's own vision system can simultaneously track. On a standalone headset, the
real constraints are elsewhere:

1. **Optical discrimination.** Every additional controller adds LEDs to the frame. Two
   controllers = 36 LEDs `[P]` in a fisheye image, some overlapping, some partially occluded.
   Identification cost grows faster than linearly.
2. **Compute.** Blob detection + multi-camera PnP + fusion per controller per frame, on mobile
   silicon, inside a ~11 ms budget at 90 Hz. `[I]`
3. **Radio capacity.** 2.4 GHz is shared. Valve's own support page warns about IR and RF
   interference in the room `[D]`. More controllers means more radio scheduling.
4. **Role model.** OpenXR is built around two hands. Extra tracked objects need to be
   addressed as generic tracked devices, which is a runtime/driver question, not a vision one.
   `[I]`

So the honest framing is: **64 is the software ceiling; the Frame's vision+radio ceiling is
somewhere at or below that and we have to measure it.**

## Fresh evidence from the XRService binary `[V] 2026-11-11`

Extracting option names from `/opt/steamvr/drivers/cv/bin/linuxarm64/XRService` turned up the
following, which change the picture:

| Option name | What it implies |
|---|---|
| `filterTrackedLedsForOtherControllers` | the tracker explicitly expects LEDs belonging to *other* controllers in the same frame, and filters them |
| `alternateNNTrackingBetweenControllers` | NN tracking is scheduled across multiple controllers |
| `estimateYawFor1LEDControllers` | there is a code path for a single-LED controller |
| `enableFewLedTrackingMode` | a sparse-constellation mode exists |
| `fallbackToPoseResidualIfNotAllLedsAreVisbled` | partial visibility is a designed-for case |
| `controllerTrackingOptions`, `controllerFeatures`, `minControllerSettings` | controller tracking is a configurable subsystem, not a hard-coded pair |

None of these are documented, and none prove a numeric ceiling. But "filter LEDs for other
controllers" is strong evidence the optical layer was built to see **more than the two it
owns**. That supports the prediction below.

## What would settle it, in order of cost

### Cheap, static (done 2026-11-11)

Enumerating the stack found **no hard numeric controller limit** — but it did find the
multi-controller machinery listed above. The relevant places to look again after an OS update:

```bash
# On the Frame
ls /opt/steamvr/bin/linuxarm64/
strings /opt/steamvr/drivers/cv/bin/linuxarm64/XRService \
  | grep -E "^[a-z][A-Za-z0-9]{4,44}$" | sort -u > /tmp/xrs_opts.txt
grep -iE "controller|led|pair" /tmp/xrs_opts.txt
grep -iE "controller|tracked device" ~/.local/share/Steam/logs/vrserver.txt | tail -30
```

`vrserver.txt` records pairing and device events (`cv: CRoyPairing ...`,
`Using existing HMD ...`). It did **not** show any device-count ceiling.

### Cheap, empirical (needs controllers only)

1. Power on both Frame controllers. Confirm 2 tracked devices in SteamVR status.
2. Power on a third emitter source — even a bare LED rig — in view of the cameras.
3. Watch whether the headset enumerates an extra anonymous device, logs an unknown blob, or
   ignores it entirely.

That single test distinguishes "the vision system has spare capacity" from "it only looks for
known models", and it costs nothing but an LED and a battery.

### Expensive (needs hardware we don't have)

Borrow a second pair of Frame controllers and repeat. Pairing two identical pairs may itself
be impossible by design (each controller is bound to one headset) `[U]`.

## Prediction to test

My working guess `[I]`: the optical layer can track several rigid bodies at once without much
extra cost — blob detection and PnP scale gracefully — but the **binding** layer (which
controller is player 1) and the **radio** layer are where the practical limit sits. That would
mean the Frame could *see* four controllers and still only *use* two.

If that's true, the number the user is really after ("more than 2") is achievable for custom
rigs faster than for extra commercial controllers.

## What we record

Put results in `experiments/EXP-002-tracking-ceiling.md` with:

- build number and date
- number of emitters in view
- what SteamVR status showed
- relevant log excerpts
- conclusion + confidence tag
