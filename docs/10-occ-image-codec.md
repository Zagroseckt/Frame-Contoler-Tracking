# OCC camera video: validated reference extraction

Investigated 2026-11-12 using the local `Deckard_2026-11-12_00-16-36.dataset` copy. Every copied file passed SHA-256 comparison with the headset. No headset operations were performed by the codec investigator. This is an observed format, not a general OCC specification.

## Result

The image data is **H.264/AVC video**, not independently compressed still images. FFmpeg decoded every frame from all six extracted streams without reporting errors. A visually inspected camera 1 frame shows a coherent fisheye room view with monitor, arms and controllers. Camera 5's first frame is almost black; that is consistent with a short-exposure controller view, not evidence of failed decoding. Video is lossy; do not call the output raw sensor imagery.

The six logical streams are not six distinct physical cameras. Comparison of the field 55 serial strings with `snapshots/BeginningSnapshot/cameraMetadata.json` gives:

| OCC field 15 | Record | Physical camera | View | Size | Frames |
|---|---:|---|---|---|---:|
| 1 | 32 | slam_right | SLAM | 1056 x 1024 | 1643 |
| 2 | 32 | slam_left | SLAM | 1056 x 1024 | 1643 |
| 3 | 38 | slam_right | controller exposure | 1056 x 1024 | 1642 |
| 4 | 38 | slam_left | controller exposure | 1056 x 1024 | 1642 |
| 5 | 38 | upper_left | controller exposure | 640 x 480 | 1642 |
| 6 | 38 | upper_right | controller exposure | 640 x 480 | 1642 |

These names/IDs are dataset-specific observations, not guaranteed enum values in other recordings. The snapshot metadata is captured at a nearby time and does not itself prove exact image correspondence. The SLAM and controller images from the same physical sensors are separate, alternating exposures, not pyramid levels or duplicate encodings.

## Layout and metadata

Each record begins with ASCII `occipitl`, followed by little-endian unsigned 32-bit type and payload length. Types 32 and 38 contain a flat sequence of little-endian `u32 field, u32 length, bytes` TLVs. The **first** field 1 is a four-byte count: 2 for observed type 32, 4 for type 38. It is followed by that many concatenated camera field groups. Each observed group starts with field 19, and includes its own field 1 (44 bytes), field 15, repeated field 14, field 13, and other metadata. Do not collapse TLVs into a single dictionary: fields recur both across and within groups. The reference extractor checks the observed boundaries and fails closed if they differ.

For this dataset:

- Group field 1 starts with little-endian `u32 width, u32 height`; the remaining 36 bytes are not decoded.
- Field 15 is the logical camera ID. Field 55 contains a NUL-terminated physical-camera serial in a 65-byte field.
- Field 14 repeats twice, carrying bare H.264 SPS and PPS NALs (types 7 and 8). These are **not** length-prefixed inside their TLVs.
- Field 13 holds one or more `u32 big-endian NAL length, NAL bytes` units; all entries in this recording held one, type 5 (IDR) or type 1 (non-IDR). The initial bytes `00 03 18 d4 65 ...` mean 202,964 bytes after the four-byte length, beginning with IDR header `65`.
- To create a decoder-ready Annex B stream, output `00 00 00 01` before each field 14 NAL and before each field 13 NAL. Keep each logical camera in its own stream and preserve record order. Never concatenate different cameras into one video stream.
- Per camera there are 28 IDRs and about 1,614–1,615 non-IDR pictures. Camera 1 decoded as 28 I and 1,615 P pictures, no B pictures. An individual P-frame blob cannot generally be decoded alone: start with its preceding IDR and intervening pictures. Field 14 was present in the observed groups, but other recordings may differ.
- Field 3 is an f64 in the same apparent seconds clock domain as snapshot `timestampSec` (first camera 1: 47466.622603); timestamp meaning and alignment to tracked poses are not independently established. Field 6 is a u64 frame-like sequence, advancing by 2 per logical stream in this sample. Field 4 is an f64 matching snapshot exposure milliseconds divided by 1000; field 11 is an f32 matching snapshot gain; field 33 is an f32 matching snapshot temperature Celsius (NaN for upper sensors). These are strong cross-checks, not a published schema. Fields 12 and 36 are additional f64 time-like values, semantics unknown. Field 37 is an index 0–3 in the observed camera groups. The tool preserves these as numbered fields; unknown/nonfinite numeric values are JSON null.
- Intrinsics, poses and other tracking metadata in fields 23–27 and 48 are not decoded. The dataset also contains separate `liveTrackedStates` and controller `.ocsv` files, including timestamps and pose headers; interpreting their binary payload and aligning clocks remains future work. No pose has been claimed as ground truth here.

## Reproduce

Python 3 and FFmpeg are needed; the extractor uses only the Python standard library. The output directory must not already exist, to avoid overwriting work. From the project root:

```bash
python3 tools/occblob.py local/datasets/Deckard_2026-11-12_00-16-36.dataset/Deckard_2026-11-12_00-16-36.occ /tmp/opencode/occ-video
ffmpeg -v error -i /tmp/opencode/occ-video/camera-1.h264 -frames:v 1 /tmp/opencode/occ-video/camera-1-first.png
```

Output includes `camera-1.h264` through `camera-6.h264` and one `metadata.jsonl` row per group, in input order. The Annex B files do **not** embed the original timestamps; use the metadata, not FFmpeg's synthetic video timing, for synchronization. Frame order/count correspondence has been checked for this recording, but more complex H.264 reordering in other files would need explicit timestamp handling. Use `--allow-truncated` only for a deliberately cut prefix; it ignores an incomplete last record, not corrupt complete records. Failed extraction can leave a partial output directory.

Validation: parsing the complete local OCC yielded 9,854 groups. `ffmpeg -v error -i camera-N.h264 -f null -` succeeded for all six; `ffprobe -count_frames` returned the exact per-camera counts in the table and `yuvj420p` output. H.264 decode output may be converted to grayscale for display, but this is not a promise of sensor-native values. SHA-256 checks confirmed the copied files match the source.
