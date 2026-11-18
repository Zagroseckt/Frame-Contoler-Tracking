#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build="$root/local/viewer-build"
input="$root/local/datasets/Deckard_2026-11-12_00-16-36.dataset/Deckard_2026-11-12_00-16-36.occ"
umask 077
cmake -S "$root" -B "$build"
cmake --build "$build" --parallel
if [[ $# -gt 0 ]]; then
    exec "$build/tracking-viewer" "$@"
fi
for tool in python3 ffmpeg; do
    command -v "$tool" >/dev/null || { printf 'Missing required program: %s\n' "$tool" >&2; exit 1; }
done
[[ -f "$input" ]] || { printf 'The local recording was not found: %s\n' "$input" >&2; exit 1; }
[[ -d /dev/shm && -w /dev/shm ]] || { printf 'RAM-backed /dev/shm is unavailable. No frames were written to disk.\n' >&2; exit 1; }
ramdir="$(mktemp -d /dev/shm/sfct-viewer-XXXXXXXX)"
trap 'rm -rf -- "$ramdir"' EXIT
printf 'Preparing six camera streams in temporary RAM storage. This can take a few minutes.\n'
python3 "$root/tools/occblob.py" "$input" "$ramdir/streams"
for camera in {1..6}; do
    printf 'Preparing camera %s of 6 in RAM...\n' "$camera"
    mkdir "$ramdir/camera-$camera"
    ffmpeg -nostdin -v error -i "$ramdir/streams/camera-$camera.h264" -fps_mode passthrough -pix_fmt gray -compression_level 1 "$ramdir/camera-$camera/frame_%06d.png"
    [[ -f "$ramdir/camera-$camera/frame_000001.png" ]] || { printf 'Camera %s produced no frames.\n' "$camera" >&2; exit 1; }
done
printf 'Opening viewer. RAM files are removed when the viewer closes.\n'
"$build/tracking-viewer" --cameras "$ramdir"
