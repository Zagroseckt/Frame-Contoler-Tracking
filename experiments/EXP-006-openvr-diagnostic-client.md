# EXP-006 — Custom OpenVR diagnostic client (`sfct-diag`)

- Date: 2026-11-11 (planned)
- Frame build: SteamVR 2026-09-23 23:30:05, SteamOS aarch64, HMD `FPRAH62100F49` (`Deckard MP` rev 6)
- Hardware in play: Frame + both Roy controllers
- Status: planned

## Hypothesis

A stock OpenVR client attaching to the running `vrserver` can read every tracked device's pose
and tracking state with **zero** impact on normal Steam use, and is therefore the correct
foundation for a diagnostic interface. A second tracking instance is not possible (see
`docs/08`), so the client is the only viable shape.

## Procedure

1. Build on-device (toolchain already present: g++ 15.1.1, cmake 3.29.2, make 4.4.1):

   ```bash
   mkdir -p ~/sfct && cd ~/sfct
   g++ -O2 -std=c++17 sfct_diag.cpp \
     -I/opt/steamvr/tools/hellovr_vulkan_linux/src/openvr/headers \
     -L/opt/steamvr/bin/linuxarm64 -lopenvr_api \
     -Wl,-rpath,/opt/steamvr/bin/linuxarm64 \
     -o sfct_diag
   ```

2. Wake both controllers. Confirm with `vrcmd --info` that three devices enumerate.
3. Run `./sfct_diag` for 60 s. Dump one CSV row per device per sample:
   `t, idx, class, serial, valid, trackingResult, px, py, pz, qx, qy, qz, qw`.
4. Watch for: nothing in `vrserver.txt` from our client, no tracking dropouts, no camera errors
   in the `XRService` log.
5. Repeat movements: slow sweep, fast flick, controller occluded behind the torso, controller
   held near the face (inside the upper-camera FOV), and controller at the edge of the room.

## Result

Pending.

## Evidence

Pending — target CSV in `captures/poses/` (git-ignored), summary table in this file.

## Conclusion

Pending.

## Next

- Compare per-device `trackingResult` transitions against visible LED count.
- Feed the CSV into the optical-side comparison once our own solver exists.

## Notes on safety

This experiment is read-only by construction: `VR_Init` with `VRApplication_Background`, poses
pulled via `GetDeviceToAbsoluteTrackingPose`, no input, no overlays, no config writes. Compare
with `trackingtest`, which does exactly this and has already been run live.
