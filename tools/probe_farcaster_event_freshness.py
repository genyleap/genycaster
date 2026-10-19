#!/usr/bin/env python3
"""Diagnose chronological discovery availability on a Farcaster Snapchain node.

This operator-only probe reads public node information and a bounded recent
block window per shard. It prints aggregate recency, never cast text, hashes,
author IDs or API credentials. It does not modify the node or application.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import urlopen

FARCASTER_EPOCH = dt.datetime(2021, 1, 1, tzinfo=dt.timezone.utc)
BLOCK_EVENT_SHIFT = 14


def fetch_json(base_url: str, path: str, timeout: float) -> dict:
    with urlopen(base_url.rstrip("/") + path, timeout=timeout) as response:
        return json.load(response)


def probe(base_url: str, depth: int, timeout: float, check_hydration: bool = False) -> int:
    info = fetch_json(base_url, "/v1/info", timeout)
    now = dt.datetime.now(dt.timezone.utc)
    total_casts = 0
    recent_hashes = []
    for shard in info.get("shardInfos", []):
        shard_id = int(shard["shardId"])
        # Shard 0 contains the on-chain identity stream, not user casts.
        if shard_id == 0:
            continue
        height = int(shard["maxHeight"])
        start_id = max(0, height - depth) << BLOCK_EVENT_SHIFT
        path = "/v1/events?" + urlencode(
            {"from_event_id": start_id, "shard_index": shard_id}
        )
        try:
            events = fetch_json(base_url, path, timeout).get("events", [])
        except (HTTPError, URLError, TimeoutError, ValueError) as error:
            print(f"shard={shard_id} error={type(error).__name__}")
            continue

        ages = []
        for event in events:
            if event.get("type") != "HUB_EVENT_TYPE_MERGE_MESSAGE":
                continue
            message = event.get("mergeMessageBody", {}).get("message", {})
            data = message.get("data", {})
            if data.get("type") != "MESSAGE_TYPE_CAST_ADD":
                continue
            if data.get("castAddBody", {}).get("parentCastId"):
                continue
            seconds = data.get("timestamp")
            if not isinstance(seconds, int):
                continue
            age = (now - (FARCASTER_EPOCH + dt.timedelta(seconds=seconds)))
            ages.append(age.total_seconds() / 60)
            cast_hash = message.get("hash", "")
            if isinstance(cast_hash, str) and len(cast_hash) == 42:
                recent_hashes.append((seconds, cast_hash))
        total_casts += len(ages)
        youngest = f"{min(ages):.1f}" if ages else "none"
        print(f"shard={shard_id} height={height} window_blocks={depth} "
              f"events={len(events)} root_casts={len(ages)} "
              f"newest_age_minutes={youngest}")
    print(f"total_root_casts={total_casts}")
    if check_hydration and recent_hashes:
        unique_hashes = list(dict.fromkeys(
            cast_hash for _, cast_hash in sorted(recent_hashes, reverse=True)
        ))[:30]
        path = "/v2/farcaster/cast/bulk?" + urlencode(
            {"hashes": ",".join(unique_hashes)}
        )
        hydrated = fetch_json(base_url, path, timeout)
        print(f"requested_hydration={len(unique_hashes)} "
              f"hydrated_casts={len(hydrated.get('casts', []))}")
    return 0 if total_casts else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default="https://haatz.quilibrium.com")
    parser.add_argument("--depth", type=int, default=40)
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--check-hydration", action="store_true",
                        help="Check the bounded bulk cast hydration contract")
    args = parser.parse_args()
    if args.depth < 1 or args.depth > 1000:
        parser.error("--depth must be between 1 and 1000")
    if not args.base_url.startswith(("https://", "http://")):
        parser.error("--base-url must be HTTP(S)")
    return probe(args.base_url, args.depth, args.timeout, args.check_hydration)


if __name__ == "__main__":
    raise SystemExit(main())
