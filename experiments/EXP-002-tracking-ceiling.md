# EXP-002 — Tracking ceiling: does the Frame notice a third emitter?

- Date: (planned)
- Frame build / SteamOS build: (fill from `cat /etc/os-release` + SteamVR version)
- Hardware in play: 2 Frame controllers, one bare IR LED rig on a battery
- Status: planned

## Hypothesis

The Frame's vision system can *see* more than two emitter groups but will only *bind* the two it
recognises. A third, unknown emitter should either be ignored or appear as an anonymous tracked
object. Either outcome is informative about the ceiling.

## Procedure

1. Baseline: power both Frame controllers. Confirm 2 tracked devices in SteamVR status.
2. Build a minimal IR emitter: one or a few 850/940 nm IR LEDs, resistor, coin cell, mounted on a
   small board. (Do not blind yourself; keep it low power and indirect.)
3. Hold it in view of the headset alongside the controllers.
4. Observe: SteamVR device list, any "unknown device" entries, log lines about blobs/constellations.
5. Repeat with 2 and 4 emitters to see whether pairs get grouped.

## Safety

- Never look into an IR LED at close range, even if invisible. IR eye damage is real and painless.
- Keep emitter power modest; the Frame's cameras are sensitive.

## Result

(pending)

## Evidence

(pending — log excerpts, device list screenshots via headset view)

## Conclusion

(pending)

## Next

If anonymous blob tracking exists, `docs/04`'s ceiling question gets a real number.
If not, the ceiling is a *binding* limit, not a vision limit — which is a much better problem.
