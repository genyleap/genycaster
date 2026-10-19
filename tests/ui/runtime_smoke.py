#!/usr/bin/env python3

import http.server
import json
import pathlib
import subprocess
import sys
import tempfile
import threading
from urllib.parse import parse_qs, urlsplit


RECENT_HASH = "0x" + "a" * 40

CAST = {
    "object": "cast",
    "hash": "0xabc123",
    "parent_hash": "0xparent",
    "author": {
        "fid": 42,
        "username": "alice",
        "display_name": "Alice",
        "custody_address": "0x1234",
        "pfp_url": "",  # Keep this local read test independent of external TLS/image I/O.
        "profile": {"bio": {"text": "local runtime smoke"}},
    },
    "text": "The routed runtime reached the presentation shell. \U0001F440 \U0001F525",
    "timestamp": "2026-08-27T12:34:56.789Z",
    "embeds": [],
}

RECENT_CAST = {**CAST, "hash": RECENT_HASH, "parent_hash": None}

USER = {
    "object": "user",
    "fid": 42,
    "username": "alice",
    "display_name": "Alice",
    "custody_address": "0x1234",
    "pfp_url": "",  # Keep this local read test independent of external TLS/image I/O.
    "registered_at": "2026-08-27T00:00:00.000Z",
    "profile": {"bio": {"text": "local runtime smoke"}},
    "follower_count": 12,
    "following_count": 8,
    "verifications": [],
    "auth_addresses": [],
    "verified_addresses": {
        "eth_addresses": [],
        "sol_addresses": [],
        "primary": {},
    },
    "verified_accounts": [],
}


class Handler(http.server.BaseHTTPRequestHandler):
    paths: list[str] = []

    def do_GET(self) -> None:
        type(self).paths.append(self.path)
        if self.path == "/v1/info":
            response = {"shardInfos": [
                {"shardId": 0, "maxHeight": 1000},
                {"shardId": 1, "maxHeight": 1000},
                {"shardId": 2, "maxHeight": 1000},
            ]}
        elif self.path.startswith("/v1/events?"):
            query = parse_qs(urlsplit(self.path).query)
            shard = query.get("shard_index", [""])[0]
            response = {"events": [{
                "id": (900 << 14) + 1,
                "blockNumber": 900,
                "type": "HUB_EVENT_TYPE_MERGE_MESSAGE",
                "mergeMessageBody": {"message": {
                    "hash": RECENT_HASH,
                    "data": {"type": "MESSAGE_TYPE_CAST_ADD",
                             "timestamp": 182600000,
                             "castAddBody": {"text": "recent root"}},
                }},
            }] if shard == "1" else []}
        elif self.path.startswith("/v2/farcaster/cast/bulk?"):
            response = {"casts": [RECENT_CAST], "next": {"cursor": None}}
        elif self.path.startswith("/v2/farcaster/feed/trending?"):
            response = {"casts": [CAST], "next": {"cursor": None}}
        elif self.path.startswith("/v2/farcaster/feed/following?"):
            response = {"casts": [CAST], "next": {"cursor": None}}
        elif self.path == "/v2/farcaster/user/by-username?username=alice":
            response = {"user": USER}
        elif self.path == "/v2/farcaster/user?fid=42":
            response = {"user": USER}
        else:
            self.send_error(404)
            return
        payload = json.dumps(response).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, format: str, *args: object) -> None:
        del format, args


def run_client(
    executable: str,
    base_url: str,
    database: pathlib.Path,
    *extra_arguments: str,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            executable,
            "--runtime-smoke-test",
            f"--hypersnap-base-url={base_url}",
            f"--database-path={database}",
            *extra_arguments,
        ],
        check=False,
        capture_output=True,
        text=True,
        timeout=20,
    )


def report_failure(completed: subprocess.CompletedProcess[str]) -> int:
    print(completed.stdout, file=sys.stderr)
    print(completed.stderr, file=sys.stderr)
    return completed.returncode


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: runtime_smoke.py <farcaster-client>", file=sys.stderr)
        return 2

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="genycaster-ui-runtime-") as directory:
            temporary = pathlib.Path(directory)
            base_url = f"http://127.0.0.1:{server.server_port}"

            database = temporary / "recent.sqlite3"
            Handler.paths.clear()
            completed = run_client(sys.argv[1], base_url, database, "--open-explore")
            if completed.returncode != 0:
                return report_failure(completed)
            if not database.exists():
                print("runtime did not create the local SQLite database", file=sys.stderr)
                return 3
            expected_recent = (
                "/v1/info", "/v1/events?from_event_id=",
                "/v2/farcaster/cast/bulk?hashes=",
            )
            if not all(any(path.startswith(prefix) for path in Handler.paths)
                       for prefix in expected_recent):
                print(f"runtime did not hydrate confirmed recent casts: {Handler.paths}",
                      file=sys.stderr)
                return 4

            home_database = temporary / "home.sqlite3"
            settings = temporary / "settings.ini"
            Handler.paths.clear()
            completed = run_client(
                sys.argv[1],
                base_url,
                home_database,
                "--auto-open-identity=alice",
                f"--settings-path={settings}",
            )
            if completed.returncode != 0:
                return report_failure(completed)
            first_home_paths = list(Handler.paths)
            required_home_paths = {
                "/v2/farcaster/user/by-username?username=alice",
                "/v2/farcaster/user?fid=42",
                "/v2/farcaster/feed/following?fid=42&limit=20",
            }
            if not required_home_paths.issubset(first_home_paths):
                print(
                    f"identity-to-Home requests were incomplete: {first_home_paths}",
                    file=sys.stderr,
                )
                return 5
            if any("/feed/trending?" in path for path in first_home_paths):
                print("identity-to-Home unexpectedly requested Trending", file=sys.stderr)
                return 6
            if not settings.exists():
                print("runtime did not persist the isolated settings file", file=sys.stderr)
                return 7

            Handler.paths.clear()
            completed = run_client(
                sys.argv[1],
                base_url,
                home_database,
                f"--settings-path={settings}",
                "--open-explore",
            )
            if completed.returncode != 0:
                return report_failure(completed)
            guest_restart_paths = list(Handler.paths)
            if not any(path.startswith("/v1/events?") for path in guest_restart_paths):
                print(f"guest restart did not reopen Recent: {guest_restart_paths}", file=sys.stderr)
                return 8
            if any("/feed/following?" in path for path in guest_restart_paths):
                print("guest restart incorrectly restored a browsed identity as Home", file=sys.stderr)
                return 9
    finally:
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
