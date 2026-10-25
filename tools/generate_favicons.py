#!/usr/bin/env python3
"""Regenerate browser favicons and PWA touch icons from original Genycaster SVG.

Requires ImageMagick 'convert'; never use the splash-only white mark as favicon.
"""
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[1]/"ui"/"wasm"
svg=root/"genycaster-icon.svg"
exe=shutil.which("convert")
if not exe or not svg.is_file(): raise SystemExit("ImageMagick and brand source required")
for size,filename in [
    (16,"favicon-16x16.png"),(32,"favicon-32x32.png"),(48,"favicon-48x48.png"),
    (180,"apple-touch-icon.png"),(192,"genycaster-192.png"),(512,"genycaster-512.png")]:
    subprocess.run([exe,"-background","none","-density","256",str(svg),
       "-resize",f"{size}x{size}!", "-strip",str(root/filename)],check=True)
shutil.copyfile(root/"apple-touch-icon.png",root/"apple-touch-icon-precomposed.png")
subprocess.run([exe,str(root/"favicon-16x16.png"),str(root/"favicon-32x32.png"),
    str(root/"favicon-48x48.png"),"-define","icon:auto-resize=48,32,16",
    str(root/"favicon.ico")],check=True)
print("Created browser and touch assets from genycaster-icon.svg")
