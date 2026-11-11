# 02 — Pairing and binding, layer by layer

## Why this file exists

"Pair the controller" sounds like one operation. It is at least three, and the Frame's own
pairing flow only covers one of them. Confusing them is how projects lose a week.

---

## Layer A — Optical identity (the constellation)

The headset must be able to say "those 18 dots are controller A, and dot so-and-so is at
position (x,y,z) on its body."

This requires the vision system to have a **model** of the controller: LED count, 3D layout,
and the encoding that tells two identical controllers apart.

- For Frame controllers, that model ships with the headset `[I]`.
- For a Quest 3 controller, **no model exists on the Frame.** Nothing in Valve's docs suggests
  a plugin point for third-party constellations `[D]/[U]`.

So this layer is not "pairing" at all — it's shipping the headset a new object model, which
in practice means either (a) finding an undocumented model-loading path, or (b) doing our own
pose solve outside the headset's pipeline. See `ROADMAP.md` P2.

## Layer B — Radio identity (the link)

Buttons, sticks, capacitive touch, battery state, haptics, and IMU samples travel over
**proprietary 2.4 GHz** `[I]`.

Frame behaviour per Valve's support page:

- Controllers auto-pair with the headset the first time they're powered on `[D]`.
- If one becomes unpaired, re-pairing is offered through on-screen prompts `[D]`.

That is a headset-side flow, not a user-visible protocol. There's no documented "add a
third-party controller" screen, and no SDK surface for it `[D]`.

Quest 3 pairing, for contrast, is a phone-app flow: hold Meta+B (right) or Menu+Y (left),
then confirm in the Meta Horizon app `[D]`. The controller advertises, the app mediates, the
headset binds. Two closed ecosystems with different trust roots.

**Implication:** even if we solved optics perfectly, we would still have LEDs that light up
and a controller whose buttons go nowhere.

## Layer C — Session binding (which device is player 1)

Once a controller is known optically and connected by radio, the runtime decides its role:
left hand, right hand, extra tracked object, or ignored.

- OpenVR exposes `k_unMaxTrackedDeviceCount = 64` device slots `[D]` (in `openvr.h`).
- OpenXR's action system is role-based (`/user/hand/left`), not identity-based `[D]`.
- Valve's input doc says the *HMD* type is `"frame_hmd"` but warns not to use it to infer
  controller type, because users mix and match HMDs and controllers `[D]`.

This layer is the friendliest one for us: if we can present *something* as a tracked device
with poses and inputs, the runtime's role assignment is largely a config problem.

---

## The three-layer table

| Layer | Frame's mechanism | Third-party controller needs | Difficulty |
|---|---|---|---|
| A. Optical model | Built-in constellation model per controller type `[I]` | A model the headset accepts, or bypass the headset | High |
| B. Radio link | Proprietary encrypted 2.4 GHz, auto-pair + re-pair prompts `[D]` | Protocol reverse engineering + a radio stack | Very high |
| C. Session binding | OpenVR 64 slots / OpenXR roles `[D]` | A driver that publishes poses + inputs | Moderate |

## Practical order of attack

1. **Observe A.** Get IR imagery of both controller families and compare constellations. This
   is cheap, safe, and answers "is the Frame even seeing my Quest controllers?" (`docs/05`,
   `docs/06`).
2. **Bypass A.** Build our own blob→pose pipeline against captured frames (P2). This gives
   pose without needing Valve's model.
3. **Bypass B, partially.** Accept pose-only first. Add buttons later via whatever radio path
   we can actually crack, or via a fallback input device.
4. **Land it in C.** Publish to OpenXR/OpenVR as a tracked device.

## Questions to settle on-device

1. Does the Frame log unknown LED constellations, or silently drop them? Logs may tell us.
2. Are there any controller model/config files on disk (JSON, protobuf) we could study? `[U]`
3. Does the pairing flow accept a controller with an unknown vendor ID? `[U]`
4. What does the headset do when it sees LED blobs it can't attribute — ignore, or track as
   anonymous? `[U]` (A useful experiment: point the cameras at a bare LED rig.)
