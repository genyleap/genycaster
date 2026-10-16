#!/usr/bin/env python3
"""Read-only live Hypersnap feed capability diagnostic (no auth credentials)."""

import argparse
import json
import socket
import sys
import urllib.error
import urllib.request


def probe(base: str, path: str, timeout: float):
    request = urllib.request.Request(
        base.rstrip("/") + path,
        headers={"Accept": "application/json", "User-Agent": "Genycaster-FeedProbe/1.0"},
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        if response.status != 200:
            raise ValueError(f"HTTP {response.status}")
        body = json.load(response)
    if not isinstance(body, dict) or not isinstance(body.get("casts"), list):
        raise ValueError("Invalid Farcaster v2 feed shape: expected casts array")
    return len(body["casts"]), bool((body.get("next") or {}).get("cursor"))


def main() -> int:
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument("--base-url", default="https://haatz.quilibrium.com")
    args = arguments.parse_args()
    probes = [
        ("profile casts", "/v2/farcaster/feed/user/casts?fid=3&limit=2", 8),
        ("trending", "/v2/farcaster/feed/trending?limit=2", 4),
        ("community fallback", "/v2/farcaster/feed/following?fid=3&limit=20", 12),
    ]
    states = {}
    for name, path, deadline in probes:
        try:
            casts, cursor = probe(args.base_url, path, deadline)
            states[name] = casts
            print(f"{name}: HTTP 200, {casts} casts, pagination={cursor}")
        except (TimeoutError, socket.timeout, OSError, ValueError, urllib.error.URLError) as error:
            states[name] = None
            print(f"{name}: unavailable ({type(error).__name__})")
    # A working fallback is real content, never an empty sample or a fake cast.
    return 0 if states["community fallback"] and states["community fallback"] > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
