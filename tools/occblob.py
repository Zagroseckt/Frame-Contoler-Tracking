#!/usr/bin/env python3
"""Extract per-camera Annex B H.264 streams and metadata from observed OCC records."""
import argparse
import json
import math
import struct
import sys
from pathlib import Path

MAGIC = b"occipitl"
START = b"\x00\x00\x00\x01"


def fields(payload):
    pos = 0
    while pos < len(payload):
        if len(payload) - pos < 8:
            raise ValueError("incomplete TLV header")
        ident, size = struct.unpack_from("<II", payload, pos)
        pos += 8
        if size > len(payload) - pos:
            raise ValueError("TLV extends beyond record")
        yield ident, payload[pos:pos + size]
        pos += size


def scalar(group, ident, fmt):
    values = group.get(ident, [])
    if len(values) != 1 or len(values[0]) != struct.calcsize(fmt):
        return None
    value = struct.unpack(fmt, values[0])[0]
    return value if not isinstance(value, float) or math.isfinite(value) else None


def nals(data):
    pos = 0
    while pos < len(data):
        if len(data) - pos < 4:
            raise ValueError("truncated H.264 NAL length")
        size = int.from_bytes(data[pos:pos + 4], "big")
        pos += 4
        if not size or size > len(data) - pos:
            raise ValueError("invalid H.264 NAL length")
        yield data[pos:pos + size]
        pos += size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output_dir", type=Path, help="new empty output directory")
    parser.add_argument("--allow-truncated", action="store_true", help="accept incomplete final record (e.g. a file prefix)")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=False)
    streams = {}
    count = 0
    try:
        with args.input.open("rb") as source, (args.output_dir / "metadata.jsonl").open("w") as meta:
            while True:
                offset = source.tell()
                header = source.read(16)
                if not header:
                    break
                if len(header) != 16:
                    if args.allow_truncated:
                        print(f"Ignoring truncated header at {offset}", file=sys.stderr)
                        break
                    raise ValueError(f"truncated record header at {offset}")
                magic, typ, size = struct.unpack("<8sII", header)
                if magic != MAGIC:
                    raise ValueError(f"bad magic at {offset}")
                # Records may be large; discard irrelevant records without allocating them.
                if typ not in (32, 38):
                    source.seek(size, 1)
                    if source.tell() > args.input.stat().st_size:
                        if args.allow_truncated:
                            print(f"Ignoring truncated record at {offset}", file=sys.stderr)
                            break
                        raise ValueError(f"truncated record at {offset}")
                    continue
                payload = source.read(size)
                if len(payload) != size:
                    if args.allow_truncated:
                        print(f"Ignoring truncated record at {offset}", file=sys.stderr)
                        break
                    raise ValueError(f"truncated record at {offset}")
                parts = list(fields(payload))
                if not parts or parts[0][0] != 1 or len(parts[0][1]) != 4:
                    raise ValueError(f"unexpected camera-record header at {offset}")
                expected = struct.unpack('<I', parts[0][1])[0]
                groups = []
                for ident, value in parts[1:]:
                    if ident == 19:
                        groups.append({})
                    if not groups:
                        raise ValueError(f"unexpected group boundary at {offset}")
                    groups[-1].setdefault(ident, []).append(value)
                if len(groups) != expected:
                    raise ValueError(f"camera count mismatch at {offset}: {len(groups)} != {expected}")
                for group in groups:
                    camera = scalar(group, 15, '<I')
                    if camera is None or len(group.get(13, [])) != 1:
                        raise ValueError(f"missing or ambiguous camera/blob at {offset}")
                    if camera not in streams:
                        streams[camera] = (args.output_dir / f"camera-{camera}.h264").open('wb')
                    stream = streams[camera]
                    for parameter in group.get(14, []):
                        stream.write(START + parameter)
                    units = list(nals(group[13][0]))
                    for unit in units:
                        stream.write(START + unit)
                    dims = group.get(1, [])
                    width, height = struct.unpack_from('<II', dims[0]) if len(dims) == 1 and len(dims[0]) >= 8 else (None, None)
                    serials = group.get(55, [])
                    serial = serials[0].split(b'\0', 1)[0].decode('ascii', 'replace') if len(serials) == 1 else None
                    row = dict(record_offset=offset, record_type=typ, camera=camera, width=width, height=height,
                               field3=scalar(group, 3, '<d'), field4=scalar(group, 4, '<d'),
                               field6=scalar(group, 6, '<Q'), field11=scalar(group, 11, '<f'),
                               field12=scalar(group, 12, '<d'), field16=scalar(group, 16, '<B'),
                               field17=scalar(group, 17, '<B'), field33=scalar(group, 33, '<f'),
                               field36=scalar(group, 36, '<d'), field37=scalar(group, 37, '<B'),
                               serial=serial, blob_bytes=len(group[13][0]), nal_types=[x[0] & 31 for x in units],
                               field14_nal_types=[x[0] & 31 for x in group.get(14, []) if x])
                    meta.write(json.dumps(row, allow_nan=False) + '\n')
                    count += 1
    finally:
        for stream in streams.values():
            stream.close()
    print(f"Extracted {count} camera entries into {len(streams)} streams", file=sys.stderr)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
