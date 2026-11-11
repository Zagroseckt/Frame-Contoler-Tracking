# EXP-001 — Quest 3 controllers: do they emit IR when unpaired?

- Date: (planned)
- Frame build / SteamOS build: n/a for the first half (no Frame needed)
- Hardware in play: one Meta Quest Touch Plus controller, fresh AA battery, IR-sensitive camera
- Status: planned

## Hypothesis

A powered Touch Plus controller in **pairing mode** strobes its 8 IR LEDs while searching for a
headset, because that is the only way a Quest could ever find and verify it optically. If true,
we can capture the constellation without any Meta hardware.

Competing hypothesis: LEDs only emit once a tracking session is established, which would make
an unpaired controller optically dark and force us to find another emitter state.

## Procedure

1. Dark room. Controller battery removed, then reinserted.
2. Watch through an IR-sensitive camera. Confirm the *status* LED visible to the naked eye is not
   what we're capturing — we want the invisible tracking LEDs.
3. Test states in order:
   - cold / just powered
   - single press (should be 3 blinks = awake per Meta docs)
   - pairing mode: hold Meta+B (right) or Menu+Y (left) until the status LED shows the repeating
     2-blink pattern
4. In each state, record: visible LED count, steady vs strobing, approximate pattern.
5. Slow-motion capture if the camera supports it, to estimate strobe rate.

## Result

(pending)

## Evidence

(pending — frames/video go in `captures/ir/`, not committed)

## Conclusion

(pending)

## Next

If LEDs are visible unpaired → go straight to EXP-004-style geometry capture.
If dark → try pairing mode while the Frame is running and take a tracking dataset
(EXP-003) to see whether the Frame's own cameras pick up anything.
