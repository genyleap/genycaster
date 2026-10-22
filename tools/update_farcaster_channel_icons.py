#!/usr/bin/env python3
"""Refresh a static, nonpersonal channel avatar index from Farcaster public API.

This index is presentation metadata only. Signed cast parent_url always comes
from the HyperSnap channel search response, never from this index.
"""
import json
import re
import subprocess
from pathlib import Path
url="https://api.farcaster.xyz/v2/all-channels"
raw=subprocess.check_output(["curl","--fail","--silent","--show-error","--location","--max-time","25",url],timeout=30)
data=json.loads(raw)["result"]["channels"]
icons={}
for row in data:
    cid=row.get("id")
    icon=row.get("imageUrl") or row.get("fastImageUrl")
    if isinstance(cid,str) and re.fullmatch(r"[a-zA-Z0-9_-]{1,80}",cid) and isinstance(icon,str) and icon.startswith("https://") and len(icon)<800:
        icons[cid.lower()]=icon
if len(icons)<1000 or "genyleap" not in icons:
    raise RuntimeError("Public channel catalog unexpectedly incomplete")
path=Path(__file__).resolve().parents[1]/"ui/wasm/channel-icons.json"
path.write_text(json.dumps({"source":"Farcaster public channel directory","icons":icons},separators=(",",":"),ensure_ascii=False)+"\n")
print(f"Channel avatars: {len(icons)} indexed, {path.stat().st_size} bytes; GENY={icons['genyleap']}")
