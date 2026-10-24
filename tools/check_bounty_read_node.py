#!/usr/bin/env python3
"""Fail-closed, API-key-free smoke check for a user-owned Snapchain read node.

This checks one part of the Balaji client tutorial acceptance requirements.
It neither provisions a node nor proves write, seed import, or reward eligibility.
"""
from __future__ import annotations

import argparse
import ipaddress
import json
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlsplit
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener


def validated_origin(value: str) -> str:
    parts = urlsplit(value)
    if parts.scheme not in {"http", "https"} or not parts.hostname:
        raise ValueError("A fully qualified http(s) endpoint is required")
    if parts.username or parts.password or parts.query or parts.fragment:
        raise ValueError("Credentials, query parameters and fragments are forbidden")
    if parts.path not in {"", "/"}:
        raise ValueError("Specify the Snapchain origin, without a path")
    if parts.scheme == "http":
        try:
            loopback = ipaddress.ip_address(parts.hostname).is_loopback
        except ValueError:
            loopback = parts.hostname.lower() == "localhost"
        if not loopback:
            raise ValueError("Cleartext HTTP is allowed only on loopback")
    return value.rstrip("/")


class RefuseRedirects(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        return None


def request_json(origin: str, path: str, timeout: float) -> dict:
    # No credential headers or automatic redirects to managed/unknown hosts.
    req = Request(origin + path, headers={"Accept": "application/json"})
    # Ignore proxy environment variables: this must contact the chosen node.
    with build_opener(ProxyHandler({}), RefuseRedirects).open(req, timeout=timeout) as response:
        if response.status != 200:
            raise ValueError(f"{path}: unexpected HTTP {response.status}")
        data = response.read(4_000_001)
        if len(data) > 4_000_000:
            raise ValueError(f"{path}: oversized JSON response")
    parsed = json.loads(data)
    if not isinstance(parsed, dict):
        raise ValueError(f"{path}: expected JSON object")
    return parsed


def verify_info(info: dict, max_delay: int) -> dict:
    shards = info.get("shardInfos")
    if not isinstance(shards, list) or not shards:
        raise ValueError("/v1/info: shardInfos are missing")
    heights = []
    delays = []
    for shard in shards:
        if not isinstance(shard, dict):
            raise ValueError("/v1/info: invalid shard object")
        try:
            height = int(shard["maxHeight"])
            delay = int(shard["blockDelay"])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError("/v1/info: missing or invalid shard height/delay") from exc
        if height < 1 or delay < 0 or delay > max_delay:
            raise ValueError(f"/v1/info: unsynced shard (height={height}, delay={delay})")
        heights.append(height)
        delays.append(delay)
    return {"shards": len(shards), "max_block_delay": max(delays),
            "minimum_height": min(heights)}


def verify_casts(page: dict) -> int:
    messages = page.get("messages")
    if not isinstance(messages, list) or not messages:
        raise ValueError("/v1/castsByFid: no cast messages; use an FID with known casts")
    if not any(isinstance(message, dict) and isinstance(message.get("data"), dict)
               and isinstance(message["data"].get("castAddBody"), dict)
               for message in messages):
        raise ValueError("/v1/castsByFid: no valid canonical CastAdd messages")
    return len(messages)


def verify_follow_links(page: dict, fid: int, direction: str) -> int:
    messages = page.get("messages")
    if not isinstance(messages, list):
        raise ValueError("Raw links endpoint has no messages array")
    for message in messages:
        if not isinstance(message, dict) or not isinstance(message.get("data"), dict):
            raise ValueError("Raw links endpoint has malformed message data")
        data = message["data"]
        body = data.get("linkBody")
        if not isinstance(body, dict):
            raise ValueError("Raw links endpoint returned a non-LinkAdd entry")
        if body.get("type") != "follow":
            raise ValueError("Raw follow query returned a non-follow link")
        try:
            source = int(data["fid"])
            target = int(body["targetFid"])
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError("Raw links endpoint has invalid FIDs") from exc
        if source <= 0 or target <= 0 or (source if direction == "following"
           else target) != fid:
            raise ValueError("Raw links endpoint returned the wrong FID")
    return len(messages)


def run(origin: str, fid: int, max_delay: int, timeout: float,
        check_graph: bool = False) -> dict:
    if fid <= 0 or max_delay < 0 or timeout <= 0:
        raise ValueError("FID and timeout must be positive; max delay must be nonnegative")
    base = validated_origin(origin)
    info = verify_info(request_json(base, "/v1/info", timeout), max_delay)
    query = urlencode({"fid": fid, "pageSize": 10})
    count = verify_casts(request_json(base, "/v1/castsByFid?" + query, timeout))
    checks = ["sync", "raw_cast_read_without_api_key"]
    if check_graph:
        followed = verify_follow_links(request_json(base,
            "/v1/linksByFid?" + urlencode({"fid": fid, "link_type": "follow", "pageSize": 10}),
            timeout), fid, "following")
        followers = verify_follow_links(request_json(base,
            "/v1/linksByTargetFid?" + urlencode({"target_fid": fid, "link_type": "follow",
                                                 "pageSize": 10}), timeout),
            fid, "followers")
        checks.append("raw_follow_graph_without_api_key")
        info["following_links"] = followed
        info["follower_links"] = followers
    return {"result": "pass", "checked": checks,
            "read_origin": base, "fid": fid, "cast_messages": count, **info}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--read-url", default="http://127.0.0.1:3381")
    parser.add_argument("--fid", type=int, required=True,
                        help="FID known to have public CastAdd messages")
    parser.add_argument("--max-block-delay", type=int, default=9)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--check-graph", action="store_true",
                        help="Require raw followers/following endpoints to work without API keys")
    args = parser.parse_args()
    try:
        result = run(args.read_url, args.fid, args.max_block_delay, args.timeout,
                     check_graph=args.check_graph)
    except (ValueError, HTTPError, URLError, TimeoutError, OSError,
            json.JSONDecodeError) as exc:
        # Do not print the origin or raw exception details: transport errors
        # sometimes include request URLs with sensitive operator data.
        print(json.dumps({"result": "fail", "reason": str(exc)
                          if isinstance(exc, ValueError) else type(exc).__name__}))
        return 1
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
