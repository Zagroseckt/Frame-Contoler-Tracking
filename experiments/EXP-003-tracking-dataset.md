# EXP-003 — Tracking dataset with camera video

- Date: (planned)
- Frame build / SteamOS build:
- Hardware in play: Frame + both controllers
- Status: planned

## Hypothesis

Valve's "Record Tracking Data" developer feature, with camera video enabled, yields imagery of
the tracking cameras good enough to run our own blob detection — the practical equivalent of a
"filters off" view.

## Procedure (per Valve's debugging doc)

1. Wear the headset, open VR dashboard → Steam overlay → VR Settings.
2. Enable **Advanced Settings**.
3. Go to **Developer** tab, scroll to **Record Tracking Data**.
4. Enable camera video (and screen/audio if useful for narration).
5. START. Confirm recording icon appears. Close dashboard.
6. Move controllers through varied poses, including partially occluded and out of frame, ~30–60 s.
7. STOP from the same menu.
8. Locate the dataset on disk via SSH. Suggested search after capture:
   ```bash
   find ~ /home/steamos /tmp -newermt '-10 minutes' -type f 2>/dev/null | grep -iE "track|dataset|capture" | head -50
   ```
9. Copy it to the host: `scp -r frame:<path> ~/workshop/SFControlerTracking/captures/datasets/`

## Questions this answers

- Are there camera frames at all, and how many cameras?
- Raw, downscaled, or post-processed?
- Is there per-frame pose data we can use as ground truth for our own solver?

## Result

(pending)

## Evidence

(pending — dataset inventory: file names, sizes, frame counts; sample frames in `captures/`)

## Conclusion

(pending)

## Next

If frames are usable → `ROADMAP.md` P2.1 blob detector runs against them offline.
If frames are absent or too processed → rely on the independent IR rig and `/dev/video99`.
