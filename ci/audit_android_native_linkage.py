#!/usr/bin/env python3
"""Fail closed on Android ELF linkage regressions, before signing/publication."""
from __future__ import annotations
import argparse
import collections
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

NDK_REVISION = "30.0.16248370"
HASH_SYMBOL = "_ZNSt6__ndk113__hash_memoryEPKvm"
# Public Android NDK libraries; never accept arbitrary unresolved SONAMEs.
PLATFORM = frozenset(("libc.so", "libm.so", "libdl.so", "liblog.so",
    "libandroid.so", "libjnigraphics.so", "libz.so", "libEGL.so",
    "libGLESv2.so", "libGLESv3.so", "libOpenSLES.so", "libaaudio.so",
    "libmediandk.so", "libvulkan.so", "libnativewindow.so", "libcamera2ndk.so",
    "libsync.so", "libbinder_ndk.so"))
PRIVATE_PREFIXES = ("_ZN", "_ZT", "av_", "avcodec_", "avformat_", "avutil_",
    "sws_", "swr_", "SSL_", "OPENSSL_", "CRYPTO_", "EVP_")

def readelf(tool, path, *args):
    return subprocess.check_output([str(tool), *args, str(path)], text=True, timeout=30)

def symbols(tool, path):
    exported, imported = set(), set()
    for line in readelf(tool, path, "--dyn-syms", "--wide").splitlines():
        fields = line.split()
        if len(fields) < 8 or not re.fullmatch(r"\d+:", fields[0]):
            continue
        _, _, _, _, bind, visibility, index, name, *_ = fields
        if bind not in ("GLOBAL", "WEAK", "UNIQUE", "GNU_UNIQUE"):
            continue
        if index == "UND":
            if bind == "GLOBAL":
                imported.add(name)
        elif visibility in ("DEFAULT", "PROTECTED"):
            exported.add(name.replace("@@", "@"))
            if "@@" in name:
                exported.add(name.split("@")[0])
    return exported, imported

def dynamic(tool, path):
    output = readelf(tool, path, "-dW")
    needed = re.findall(r"\(NEEDED\).*?\[(.*?)\]", output)
    soname = re.findall(r"\(SONAME\).*?\[(.*?)\]", output)
    return needed, soname[0] if soname else path.name

def build_id(tool, path):
    match = re.search(r"Build ID:\s*([a-fA-F0-9]+)", readelf(tool, path, "-n"))
    if not match:
        raise ValueError(f"Missing GNU build ID: {path.name}")
    return match[1].lower()

def ndk_paths(ndk):
    if not re.search(r"(?m)^Pkg\.Revision\s*=\s*"+re.escape(NDK_REVISION)+r"\s*$", (ndk/"source.properties").read_text()):
        raise ValueError("Android NDK revision differs from pinned Qt 6.12.1 toolchain")
    prebuilt = ndk/"toolchains/llvm/prebuilt/linux-x86_64"
    return prebuilt/"bin/llvm-readelf", prebuilt/"sysroot/usr/lib/aarch64-linux-android"

def sdk_check(sdk, ndk):
    tool, libdir = ndk_paths(ndk)
    core = sdk/"lib/libQt6Core_arm64-v8a.so"
    multimedia = sdk/"lib/libQt6Multimedia_arm64-v8a.so"
    for p in (core, multimedia):
        # Android ident note includes NDK build number; exact SDK build
        # observed in the distributed Qt 6.12.1 AArch64 binaries.
        if b"16248370\0" not in p.read_bytes():
            raise ValueError(f"Qt SDK/NDK provenance mismatch: {p.name}")
    if HASH_SYMBOL not in symbols(tool, libdir/"libc++_shared.so")[0]:
        raise ValueError("Selected NDK C++ runtime does not export required hash symbol")
    print("Qt SDK r30 provenance and NDK libc++ symbol verified")

def audit(apk, ndk=None):
    tool, libdir = ndk_paths(ndk) if ndk else ("readelf", None)
    with zipfile.ZipFile(apk) as z, tempfile.TemporaryDirectory(prefix="android-linkage-") as folder:
        names = [n for n in z.namelist() if n.startswith("lib/arm64-v8a/") and n.endswith(".so")]
        if not names or len(names) != len(set(names)):
            raise ValueError("Missing or duplicate Android native libraries")
        paths = {}
        for n in names:
            p = Path(folder)/Path(n).name
            if p.name in paths:
                raise ValueError(f"Duplicate native library basename: {p.name}")
            p.write_bytes(z.read(n))
            paths[p.name] = p
        if "libc++_shared.so" not in paths:
            raise ValueError("Missing bundled C++ runtime")
        exports, imports, deps = {}, {}, {}
        for n,p in paths.items():
            exports[n], imports[n] = symbols(tool,p)
            deps[n], soname = dynamic(tool,p)
            if soname != n:
                raise ValueError(f"APK library SONAME mismatch: {n}: {soname}")
        external = set(d for values in deps.values() for d in values)-paths.keys()
        unknown = external-PLATFORM
        if unknown:
            raise ValueError(f"Missing APK dependencies: {sorted(unknown)}")
        platform_exports = set()
        if libdir:
            for n in sorted(external):
                platform_exports.update(symbols(tool, libdir/"28"/n)[0])
            if build_id(tool,paths["libc++_shared.so"]) != build_id(tool,libdir/"libc++_shared.so"):
                raise ValueError("Packaged libc++_shared.so is not the pinned NDK runtime")
        missing = collections.defaultdict(list)
        # Model each dependency closure, not unrelated plugin exports.
        for n in paths:
            seen, todo = set(), [n]
            while todo:
                item = todo.pop()
                if item in seen or item not in paths:
                    continue
                seen.add(item)
                todo.extend(deps[item])
            available = set().union(*(exports[x] for x in seen), platform_exports)
            for sym in sorted(imports[n]-available):
                if libdir or sym.startswith(PRIVATE_PREFIXES) or "@Qt_" in sym:
                    missing[n].append(sym)
        if missing:
            raise ValueError("Unresolved strong ELF symbols:\n"+"\n".join(
                f"{name}: {', '.join(values)}" for name,values in missing.items()))
    print(f"PASS: {len(paths)} native libraries, dependency closure and "
          f"{'all strong imports against API 28 stubs' if ndk else 'private imports'} verified")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("apk",nargs="?",type=Path)
    parser.add_argument("--ndk",type=Path)
    parser.add_argument("--sdk",type=Path)
    args=parser.parse_args()
    if args.sdk:
        if not args.ndk: parser.error("--sdk requires --ndk")
        sdk_check(args.sdk,args.ndk)
    elif args.apk:
        audit(args.apk,args.ndk)
    else:
        parser.error("APK or --sdk required")

if __name__=="__main__":
    main()
