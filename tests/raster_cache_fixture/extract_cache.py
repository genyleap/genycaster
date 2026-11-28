#!/usr/bin/env python3
"""Extract the provider's pure cache/decode routines for an isolated Qt test.

The full provider imports the transport and worker modules. This fixture
compiles its exact cache and decode code against Qt, not a parallel model
of the algorithm. The full provider still needs the normal application build.
"""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()

def section(start, end):
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]

parts = [
    section("struct CachedImage final {", "struct FailedRequest final {"),
    "QMutex imageCacheMutex{};\nQCache<QString, CachedImage> imageCache{128 * 1024};\n",
    section("constexpr int decodedSizeQuantum", "namespace storage_policy"),
    section("[[nodiscard]] int quantizedRequestedDimension", "[[nodiscard]] bool requestIsBackedOff"),
    section("[[nodiscard]] int imageCostKiB", "// RemoteImageProvider is declared"),
    section("constexpr qint64 maximumDecodedPixelCount", "constexpr qint64 maximumDiskCacheEntryBytes"),
]
# Decode limits must be declared before the routines that refer to them.
parts.insert(0, parts.pop())
Path(sys.argv[2]).write_text("\n".join(parts))
