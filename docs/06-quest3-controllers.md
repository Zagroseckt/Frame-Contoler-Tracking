# 06 — Quest 3 (Touch Plus) controllers

The near-term target. This file is the honest assessment plus the attack plan.

## What they are

| Property | Value |
|---|---|
| Name | Meta Quest Touch Plus |
| Ships with | Quest 3, Quest 3S `[D]` |
| Tracking | Ringless optical: IR LEDs + IMU, fused with continuous hand tracking `[D]` |
| LEDs | 8 total: 7 on the face plate, 1 at the bottom of the handle `[D]` |
| Radio | 2.4 GHz, FCC-confirmed; single AA battery per controller `[D]` |
| Trigger | Two-stage analog index trigger (a Touch Plus first) `[D]` |
| Haptics | TruTouch VCM actuator `[D]` |
| Compatibility | Quest 3 / 3S only, per Meta `[D]` |
| Pairing | Phone app; hold Meta+B (right) or Menu+Y (left); LED blinks twice repeating while in pairing mode `[D]` |
| Status LED | 1 blink = paired+connected, 3 blinks = awake, 2-blink repeat = pairing mode, SOS = error `[D]` |

## Why they will not "just pair"

Three separate walls, in the order they will stop us:

**1. Radio wall.** The controllers speak Meta's encrypted 2.4 GHz protocol and are bound to a
Meta headset. There is no public complete protocol specification and no HID fallback. `[D]/[I]`

**2. Optical model wall.** The Frame's vision system looks for its own 18-LED constellation.
A Touch Plus controller presents 8 LEDs in a Meta-specific layout the Frame has no model
for. `[D]/[I]`

**3. Fusion wall.** Meta fuses LED tracking with continuous controller-free hand tracking to
cover the occlusion gaps the ringless design creates `[D]`. If the Frame lacks hand tracking
`[P]`, a Touch Plus rig tracked by our own code will be more fragile than it is on a Quest,
especially when the faceplate LEDs turn away from the cameras.

**The realistic shape of success:** pose from *our* vision pipeline, input from *whatever* radio
path we crack later. Not "the Frame accepts Quest controllers."

## Attack plan

### Step 1 — Do they emit at all? (cheapest, highest information)

Goal: find out whether a powered, unpaired Touch Plus controller lights its IR LEDs.

Why it's uncertain: on Quest, LED emission is tied to the tracking session. An unpaired
controller may strobe only while searching for a headset, or not at all. `[U]`

Method:

1. Fresh AA battery, dark room.
2. Watch through an IR-sensitive camera (phone front camera, or a webcam with the IR filter
   removed).
3. Test each state: cold, single press (3 blinks = awake), pairing mode (Meta+B / Menu+Y until
   2-blink repeat).
4. Record at slow-mo if possible to catch strobing.
5. Optional cross-check: put the controller in front of the Frame and take a tracking dataset
   with camera video enabled (`docs/05` §3) — if the Frame's cameras see 8 dots, our pipeline
   has something to chew on.

Pass condition: at least one state where LEDs are visible and countable.

### Step 2 — Radio visibility

1. `bluetoothctl` scan on the Frame and on a Linux PC with the controller in pairing mode.
2. Note: name, address, advertised services, whether it is BLE at all or a proprietary 2.4 GHz
   link that never appears in a BLE scan.
3. Compare against the known Quest pairing flow (phone app mediates) `[D]`.

Pass condition: we can state definitively whether the controller is BLE-visible or radio-opaque.

### Step 3 — Constellation capture

If Step 1 passes, capture the 8-LED geometry from multiple angles and derive 3D positions:

- front-on, both sides, top, bottom, at working distance (0.3–1.0 m)
- measure inter-LED spacing in pixels at a known distance to get metric geometry
- build the constellation model for the P2 pose pipeline

### Step 4 — Pose without Meta

Feed frames into our own blob→PnP pipeline (`ROADMAP.md` P2). Vision-only first; IMU later if
the radio ever opens.

### Step 5 — Input

Only after pose works. Options, worst to best:

- separate gamepad/keyboard for buttons while pose comes from vision (ugly but functional)
- a cracked radio path for buttons + IMU (research-grade effort)
- a donor Quest headset relaying input (adds a whole second device — probably out of scope)

## What we deliberately do not do

- We do not attempt to unpair someone else's controllers or impersonate a Meta headset for a
  device we don't own.
- We do not modify controller firmware.
- We do not rely on any single unverified press claim; each one gets a test.

## Open questions

1. Do Touch Plus LEDs emit when unpaired? `[U]` — Step 1 answers this.
2. Is the 2.4 GHz link BLE-based for pairing only, with a proprietary data channel after? `[U]`
3. Does the controller expose any standard HID descriptor over USB-C? `[U]` (cheap test: plug
   into a PC and watch `dmesg`/`lsusb`)
4. What is the LED strobe rate? `[U]`
5. Can the Frame's cameras resolve 8 dim dots at 2 m, given 18-dot controllers are the design
   target? `[U]`
