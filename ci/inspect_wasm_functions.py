#!/usr/bin/env python3
"""Inspect WASM function spans and optional name-section symbols without a build.

Offsets from browser stacks (e.g. 168053ca:0xd37dc0) are *module code
offsets*, not necessarily byte offsets in the .wasm file. The report prints
both the section code coordinates and the enclosing declared function.
"""
import argparse
from pathlib import Path


def uleb(buf, p):
    value, shift = 0, 0
    while p < len(buf):
        byte = buf[p]
        p += 1
        value |= (byte & 127) << shift
        if byte < 128:
            return value, p
        shift += 7
        if shift > 70:
            raise ValueError("ULEB128 too long")
    raise ValueError("Truncated ULEB128")


def section_headers(buf):
    p = 8
    while p < len(buf):
        section_id = buf[p]
        length, p = uleb(buf, p + 1)
        end = p + length
        if end > len(buf):
            raise ValueError("Invalid section length")
        yield section_id, p, end
        p = end


def import_function_count(buf, sections):
    for section_id, begin, end in sections:
        if section_id != 2:
            continue
        count, p = uleb(buf, begin)
        function_count = 0
        for _ in range(count):
            for _ in range(2):  # module and import field UTF-8 strings
                n, p = uleb(buf, p)
                p += n
            kind = buf[p]
            p += 1
            if kind == 0:  # function type
                _, p = uleb(buf, p)
                function_count += 1
            elif kind in (1, 2):  # table or memory limits
                if kind == 1:
                    p += 1
                flags, p = uleb(buf, p)
                _, p = uleb(buf, p)
                if flags & 1:
                    _, p = uleb(buf, p)
            elif kind == 3:  # global valtype and mutable flag
                p += 2
            elif kind == 4:  # exception tag attributes and type
                _, p = uleb(buf, p)
                _, p = uleb(buf, p)
            else:
                raise ValueError(f"Unknown import kind: {kind}")
        return function_count
    return 0


def encode_uleb(value):
    output = bytearray()
    while True:
        chunk = value & 127
        value >>= 7
        output.append(chunk | (128 if value else 0))
        if not value:
            return bytes(output)


def function_names(buf, sections):
    names = {}
    for section_id, begin, end in sections:
        if section_id != 0:
            continue
        n, q = uleb(buf, begin)
        name = bytes(buf[q:q+n])
        if name != b"name":
            continue
        q += n
        while q < end:
            kind = buf[q]
            length, q = uleb(buf, q + 1)
            boundary = q + length
            if kind == 1:
                count, q = uleb(buf, q)
                for _ in range(count):
                    index, q = uleb(buf, q)
                    size, q = uleb(buf, q)
                    names[index] = bytes(buf[q:q+size]).decode("utf-8", "replace")
                    q += size
            q = boundary
    return names


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("offsets", nargs="+", type=lambda s: int(s, 0))
    args = parser.parse_args()
    b = memoryview(args.binary.read_bytes())
    assert bytes(b[:8]) == bytes([0, 97, 115, 109, 1, 0, 0, 0])
    sections = list(section_headers(b))
    names = function_names(b, sections)
    imported_functions = import_function_count(b, sections)
    print("Bytes:", len(b), "sections:", len(sections),
          "symbols:", len(names), "imported functions:", imported_functions)
    for section_id, start, end in sections:
        if section_id == 10:
            count, p = uleb(b, start)
            print("code: section offset", start, "body count", count)
            for i in range(count):
                length, p = uleb(b, p)
                if any((p - 2 <= offset < p + length) for offset in args.offsets):
                    function_index = imported_functions + i
                    recursive_call = bytes([0x10]) + encode_uleb(function_index)
                    print("WASM function", function_index, "body index", i,
                          "span", hex(p), hex(p + length),
                          "symbol", names.get(function_index, "<stripped>"),
                          "potential self-call:", recursive_call in bytes(b[p:p+length]))
                    for offset in args.offsets:
                        if p - 2 <= offset < p + length:
                            print("  ", hex(offset), "relative", hex(offset - p),
                                  "bytes", bytes(b[max(p, offset-16):min(p+length, offset+16)]).hex(" "))
                p += length
            break
    for offset in args.offsets:
        if offset >= len(b):
            print(hex(offset), "outside file")
if __name__ == "__main__":
    main()
