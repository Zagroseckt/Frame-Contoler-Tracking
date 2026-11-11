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

## What would settle it, in order of cost

### Cheap, static (do first)

Enumerate the stack on the device and look for hard-coded maxima:

```bash
# On the Frame
ls /opt/steamvr/bin/linuxarm64/ | head -50
strings /opt/steamvr/bin/linuxarm64/vrserver 2>/dev/null | grep -iE "max.*(controller|device|tracked)" | head
strings <tracking-service-binary> | grep -iE "max|limit|count" | head
```

Also check configs:

```bash
find /opt/steamvr ~/.steam ~/.local/share/Steam -name "*.vrsettings" -o -name "vrserver.txt" 2>/dev/null | head
```

SteamVR writes `vrserver.txt` with device enumeration. Any log line containing
`tracked device` with an index number tells us how enumeration is handled.

`[U]` — untested on our build; headset was offline on 2026-11-11.

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
