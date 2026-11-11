# 01 — How the Frame tracks a controller

Written 2026-11-11. Everything here is tagged; see README for the tag key.

## The short version

The headset tracks *itself* by looking at the room. It tracks *controllers* by looking for
infrared LEDs stuck to them. Between camera frames, the controller's IMU carries the pose.

Two different problems, two different methods, one fused output.

---

## Stage 1 — Illumination

The Frame has infrared illuminators on the headset that flood the play space with IR light
`[D]` (Valve specs via press; also visible in support guidance about IR interference).

Why it matters: it makes the tracking cameras work in the dark, and it means the cameras can
use short exposures without giving away visible light. It also means **any IR source in the
room is a potential false positive** — Valve's own troubleshooting page tells users to remove
Christmas lights, high-powered LEDs, and other IR sources `[D]`.

## Stage 2 — Cameras

Four outward-facing monochrome fisheye cameras `[D]`:

- two at the top corners
- two at the front, low and widely spaced

They are grayscale computer-vision cameras, not colour passthrough `[D]`. Two of them double as
monochrome passthrough to the wearer `[D]`.

Field of view is wide and fisheye-distorted on purpose: coverage matters more than rectilinear
correctness. Consequences for us:

- LED images near the edge of frame are **stretched and distorted** — blob detection must be
  distortion-aware or work in undistorted space `[I]`.
- Cameras are not all looking at the same volume, so a controller can be seen by 2, 3, or 4
  cameras. The more cameras, the better the pose `[I]`.

## Stage 3 — Headset tracking (markerless SLAM)

The headset's own pose comes from markerless visual-inertial SLAM over the four cameras
`[D]`. Nothing for us to intercept here, but it explains a failure mode: if SLAM drifts, both
the world and the controllers drift *together*, which looks like "the room moved" rather than
"the controller is wrong" `[I]`.

## Stage 4 — Controller optics: the LED constellation

Each Frame controller carries **18 infrared LEDs** `[P]` — spread across the face, the handle,
and the bottom, so that some LEDs stay visible whatever the grip `[P]`.

> Compare: Quest 3 Touch Plus has 8 (7 face + 1 handle) `[D]`. That count difference is the
> single most important number in this project — see `docs/06`.

Three things the vision system has to get out of a set of glowing dots:

1. **Blob detection.** Which pixels are LEDs, versus IR reflections, lamps, sunlight, or a
   shiny picture frame. This is where filtering happens (threshold, size gate, shape gate,
   temporal continuity gate).
2. **Identification.** Which controller is this, and which LED is which? Two mechanisms are
   available: geometric constellation matching (the rigid arrangement is unique per model) and
   temporal encoding (LEDs blinking in a pattern over successive frames). Valve has not
   published which the Frame uses `[U]`.
3. **Pose solve.** Given known 3D LED positions on the controller and their 2D image
   positions in several cameras, solve for 6DoF rigid transform. Classic PnP / bundle
   adjustment territory `[I]`.

The "blink" the user asked about lives here, at identification. It is the mechanism that
turns anonymous dots into "this is the left controller, LED #7, and I know exactly where
LED #7 sits on the plastic."

## Stage 5 — IMU fusion

Each controller has an IMU `[D]`. Vision gives absolute pose at camera rate; the IMU gives
high-rate relative motion. A filter (almost certainly some flavour of EKF or a factor-graph
smoother) blends them `[I]`.

This is why controllers survive brief occlusion: the IMU coasts, and the coasting error grows
until vision reacquires. Valve's support copy says momentary occlusion "shouldn't cause
problems" but warns against covering sensors `[D]`.

**Design consequence for us:** if we build our own emitter rig (P2/P4), we get vision only
unless we also source IMU data. Vision-only at ~30–90 Hz with no inertial bridge will feel
noticeably worse than native.

## Stage 6 — Radio and identity

The controller's buttons, sticks, capacitive finger sensing, and haptics travel over a
proprietary 2.4 GHz link `[I]` (Valve documents the *capabilities*, not the transport).
Pairing binds a controller identity to the headset so the optical and radio paths can be
associated. See `docs/02`.

## Stage 7 — Presentation

The result lands in OpenXR as:

- Interaction profile `/interaction_profiles/valve/frame_controller_valve`, behind the
  `XR_VALVE_frame_controller_interaction` extension `[D]`
- OpenVR `controller_type = "frame_controller"`, with skeletal input for finger tracking `[D]`
- HMD type is `"frame_hmd"`, but Valve explicitly warns not to infer controller type from it
  because users mix and match `[D]`

For our project that last line is important: **Valve already expects a mixed-device world at
the identity level.** Whether the *tracking* layer tolerates foreign controllers is a
different question.

---

## What this means for the plan

- The optical layer is the one we can observe and, eventually, model. It is the attack surface.
- The radio layer is the wall. Buttons and IMU are behind it, and there is no public protocol
  spec.
- A pose-only foreign controller is a realistic intermediate result. Pose plus a separate
  working input path is the realistic end result.
- Filtering is not a black box we can toggle from the dashboard as far as we know `[U]` — but
  Valve's tracking-dataset recorder can capture camera video, which gives us the raw-ish
  imagery to build our *own* filters on. See `docs/05`.

## Open questions

1. Does the Frame identify LEDs by blink pattern, constellation geometry, or both? `[U]`
2. Are the four cameras hardware-synchronised, and at what rate? `[U]`
3. What is the LED strobe frequency and duty cycle? `[U]` — answerable with a high-frame-rate
   IR camera.
4. Does the tracking dataset's camera video preserve raw sensor frames or already-processed
   frames? `[U]` — decide by inspecting a capture.
5. Is there any developer switch that overlays detected markers on the camera view? `[U]`
