# Tracking frame viewer

Run `./run.sh` from the project directory. The launcher builds the GTK 3/CMake
viewer, extracts the six video streams from the local recording, and converts
them to temporary grayscale PNG frames in `/dev/shm` (RAM-backed storage). This
preparation may take several minutes and uses substantial memory. Files are
normally removed when the viewer exits; an abrupt kill or power failure can
interrupt cleanup. RAM-backed storage can be swapped to disk by the OS.

Use the six camera buttons to switch views, or **All 6** for a grid. The grid
aligns by frame number only; it is not timestamp-synchronized. The labels refer
to this particular dataset: 1 and 2 are scene exposures from the right and left
SLAM sensors; 3 and 4 are controller exposures from those sensors; 5 and 6 are
upper-left and upper-right controller exposures. Six streams, four sensors.

**Space** plays or pauses; **Left/Right** step; **Home/End** jump to the first
or last frame; **F** toggles fullscreen; **Q** quits. Buttons offer the same
basic controls. The adjustable FPS is a playback setting, not capture timing.
The recording is lossy H.264, not raw sensor data. No controller poses are
currently synchronized to it. Images may contain private room imagery.

The viewer also supports a synthetic demo and opening a PNG folder. Those are
not tracking data unless supplied from a recording. See `docs/10-occ-image-codec.md`
for the extraction findings. Dependencies are CMake, a C++17 compiler, GTK 3
development headers, Python 3, and FFmpeg.
