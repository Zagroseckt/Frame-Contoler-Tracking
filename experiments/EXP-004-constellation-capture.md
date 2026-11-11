# EXP-004 — Frame controller LED constellation, extracted from the headset

- Date: 2026-11-11
- Frame build: SteamVR 2026-09-23 23:30:05, SteamOS aarch64, HMD `FPRAH62100F49` (`Deckard MP` rev 6)
- Hardware: Frame + its paired controllers (asleep during session, but their model files persist)
- Status: **passed**

## Hypothesis

The headset stores a per-controller optical model on disk, in a format we can read and reuse.

## Method

Rather than reverse-engineering the binary format, we found the artefacts directly:

```
/home/steamos/.config/openvr/config/roy/6cb1df7a24e1.json   -> Roy_Right
/home/steamos/.config/openvr/config/roy/ebc3df5704f3.json   -> Roy_Left
/opt/steamvr/drivers/cv/resources/minControllerFeatureNNModel/k2v3Right.json -> Roy_K2V3_Right_CV
```

Copied to `captures/constellations/` (git-ignored) and measured with Python.

## Result

| Measure | Roy_Left / Roy_Right | Roy_K2V3_Right_CV |
|---|---|---|
| Model points | **18** | 17 |
| Normals | 18 | 17 |
| Channel map entries | none in file | 17 |
| Bounding box | 64 × 114 × 66 mm | 61 × 116 × 55 mm |
| Closest LED pair | 12.3 mm | 11.5 mm |
| Points on lower handle (y < −40 mm) | 3 | 3 |

Both retail controllers carry an IMU extrinsic block (`imu.position ≈ ∓3.0, −33.7, −16.8 mm`,
`plus_x = [1,0,0]`, `plus_z = [0,0,1]`).

Left and right are exact mirrors in x.

## Conclusion `[V]`

1. The "18 IR LEDs" press figure is **correct** for the retail controller.
2. A second revision (`K2V3`) uses 17 points — the LED count is **not** fixed across hardware.
3. The constellation, IMU placement, and handedness are all described in plain JSON keyed by
   controller serial. This is our reference geometry for any custom rig, and potentially a
   write target for the "teach the headset a new body" experiment.

## Next

- Verify the 18 points physically: count LEDs on the controller with an IR-sensitive camera.
- Determine whether the runtime regenerates `roy/<serial>.json` on every pairing (i.e. whether
  an edit would survive).
- Attempt a controlled settings experiment with `minControllerSettings` (see `docs/07` §8).
