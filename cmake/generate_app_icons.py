#!/usr/bin/env python3
"""Create installable platform icon assets from the canonical GenyCaster raster.

Requires only Python's standard library. Run after changing ui/wasm/genycaster-512.png:
    python3 cmake/generate_app_icons.py
    python3 cmake/generate_app_icons.py --check

No network downloads, external icon sets or platform-specific SDKs.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "ui/wasm/genycaster-512.png"
DEST = ROOT / "properties"
PNG_HEADER = b"\x89PNG\r\n\x1a\n"


def png_chunks(blob: bytes):
    if not blob.startswith(PNG_HEADER):
        raise ValueError("Not a PNG file")
    pos = len(PNG_HEADER)
    while pos + 12 <= len(blob):
        size = struct.unpack_from(">I", blob, pos)[0]
        name = blob[pos + 4:pos + 8]
        data = blob[pos + 8:pos + 8 + size]
        crc = struct.unpack_from(">I", blob, pos + 8 + size)[0]
        if zlib.crc32(name + data) & 0xffffffff != crc:
            raise ValueError("Corrupt PNG chunk")
        pos += size + 12
        yield name, data
        if name == b"IEND":
            break


def decode_png(blob: bytes):
    w = h = depth = color = interlace = None
    compressed = bytearray()
    for kind, data in png_chunks(blob):
        if kind == b"IHDR":
            w, h, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", data)
        if kind == b"IDAT":
            compressed.extend(data)
    if not w or not h or interlace != 0 or (depth, color) not in ((16, 4), (8, 6), (8, 2)):
        raise ValueError(f"Unsupported PNG: {w}x{h} depth={depth} type={color} interlace={interlace}")
    bpp = 4 if color in (4, 6) else 3
    stride = w * bpp
    unpacked = zlib.decompress(compressed)
    pixels = bytearray(w * h * 4)
    pos = 0
    prev = bytearray(stride)
    for y in range(h):
        filt = unpacked[pos]
        pos += 1
        row = bytearray(unpacked[pos:pos + stride])
        pos += stride
        for i in range(stride):
            a = row[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if filt == 1:
                predict = a
            elif filt == 2:
                predict = b
            elif filt == 3:
                predict = (a + b) // 2
            elif filt == 4:
                p = a + b - c
                da, db, dc = abs(p - a), abs(p - b), abs(p - c)
                predict = a if da <= db and da <= dc else b if db <= dc else c
            elif filt == 0:
                predict = 0
            else:
                raise ValueError(f"Unsupported PNG row filter: {filt}")
            row[i] = (row[i] + predict) & 255
        for x in range(w):
            src = x * bpp
            dst = (y * w + x) * 4
            if color == 4:
                # 16-bit grayscale + alpha; retain high bits on export.
                g, alpha = row[src], row[src + 2]
                pixels[dst:dst + 4] = bytes((g, g, g, alpha))
            elif color == 6:
                pixels[dst:dst + 4] = row[src:src + 4]
            else:
                pixels[dst:dst + 4] = row[src:src + 3] + b"\xff"
        prev = row
    return w, h, pixels


def resize_rgba(source, src_w, src_h, size):
    if src_w == size and src_h == size:
        return bytes(source)
    result = bytearray(size * size * 4)
    x_map = []
    for x in range(size):
        px = min(src_w - 1, max(0, (x + .5) * src_w / size - .5))
        x0 = int(px)
        x_map.append((x0, min(x0 + 1, src_w - 1), px - x0))
    for y in range(size):
        py = min(src_h - 1, max(0, (y + .5) * src_h / size - .5))
        y0 = int(py)
        y1 = min(y0 + 1, src_h - 1)
        wy = py - y0
        for x, (x0, x1, wx) in enumerate(x_map):
            out = (y * size + x) * 4
            p00 = (y0 * src_w + x0) * 4
            p01 = (y0 * src_w + x1) * 4
            p10 = (y1 * src_w + x0) * 4
            p11 = (y1 * src_w + x1) * 4
            for ch in range(4):
                top = source[p00 + ch] * (1 - wx) + source[p01 + ch] * wx
                bot = source[p10 + ch] * (1 - wx) + source[p11 + ch] * wx
                result[out + ch] = min(255, max(0, round(top * (1 - wy) + bot * wy)))
    return bytes(result)


def chunk(kind: bytes, payload: bytes):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)


def encode_png(size: int, rgba: bytes, opaque=False):
    if opaque:
        # App Store icon source must have no alpha channel.
        rgb = bytearray(size * size * 3)
        for i in range(size * size):
            r, g, b, alpha = rgba[4 * i:4 * i + 4]
            t = i * 3
            for off, c in enumerate((r, g, b)):
                rgb[t + off] = (c * alpha + 16 * (255 - alpha) + 127) // 255
        pix = bytes(rgb)
        bpp, color = 3, 2
    else:
        pix = rgba
        bpp, color = 4, 6
    stride = size * bpp
    rows = b"".join(b"\0" + pix[y * stride:(y + 1) * stride] for y in range(size))
    header = struct.pack(">IIBBBBB", size, size, 8, color, 0, 0, 0)
    return PNG_HEADER + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")


def ico(blobs):
    sizes = sorted(blobs)
    data_offset = 6 + len(sizes) * 16
    entries = []
    images = []
    for size in sizes:
        payload = blobs[size]
        entries.append(struct.pack("<BBBBHHII", size if size < 256 else 0,
                                   size if size < 256 else 0, 0, 0, 1, 32,
                                   len(payload), data_offset))
        images.append(payload)
        data_offset += len(payload)
    return struct.pack("<HHH", 0, 1, len(sizes)) + b"".join(entries) + b"".join(images)


def icns(blobs):
    mapping = {16:b"icp4", 32:b"icp5", 64:b"icp6", 128:b"ic07",
               256:b"ic08", 512:b"ic09", 1024:b"ic10"}
    entries = [kind + struct.pack(">I", len(blobs[size]) + 8) + blobs[size]
               for size, kind in mapping.items()]
    payload = b"".join(entries)
    return b"icns" + struct.pack(">I", len(payload) + 8) + payload


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--check", action="store_true",
                      help="Verify committed icon assets exactly match the master")
    verify = args.parse_args().check
    w, h, source = decode_png(SOURCE.read_bytes())
    if w != 512 or h != 512:
        raise ValueError("Expected a 512x512 source icon")
    sizes = [16, 24, 32, 48, 64, 72, 96, 128, 144, 192, 256, 512, 1024]
    blobs = {size: encode_png(size, resize_rgba(source, w, h, size))
             for size in sizes}
    outputs = {
        "macOS/genycaster.icns": icns(blobs),
        "Windows/genycaster.ico": ico({size:blobs[size] for size in (16,24,32,48,64,128,256)}),
        "Linux/genycaster.png": blobs[512],
        "iOS/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png": encode_png(
            1024, resize_rgba(source, w, h, 1024), opaque=True),
    }
    for directory, size in [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96),
                            ("xxhdpi", 144), ("xxxhdpi", 192)]:
        for name in ("ic_launcher", "ic_launcher_round"):
            outputs[f"Android/res/mipmap-{directory}/{name}.png"] = blobs[size]
    for rel, data in outputs.items():
        path = DEST / rel
        if verify:
            if not path.exists() or path.read_bytes() != data:
                raise SystemExit(f"Icon missing or stale: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    print(f"{'Validated' if verify else 'Generated'} {len(outputs)} deterministic platform icon assets.")


if __name__ == "__main__":
    main()
