#!/usr/bin/env python3
"""Launch Genycaster with a strictly pinned raw Snapchain reader and validator.

This configures *existing* node endpoints. It does not provision compute,
create Farcaster accounts, or claim that a raw node supports indexed feeds.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import sys

from check_bounty_read_node import run as inspect_node
from check_bounty_read_node import validated_origin


READ_ID = "personal-snapchain-read"
WRITE_ID = "independent-snapchain-writer"


def make_environment(read_url: str, write_url: str, inherited: dict[str, str],
                     writer_self_hosted: bool = False) -> dict[str, str]:
    reader = validated_origin(read_url)
    writer = validated_origin(write_url)
    if reader == writer:
        raise ValueError(
            "Use an independent verified validator endpoint; read nodes "
            "must not be assumed to accept canonical writes")
    env = dict(inherited)
    # The strict self-hosted example has no dependency on managed API keys.
    # Do not let unrelated legacy credentials escape to a selected raw node.
    for key in ("GENYCASTER_READ_PROVIDER_API_KEYS_JSON",
                "GENYCASTER_FARCASTER_READ_API_KEY",
                "GENYCASTER_SNAPCHAIN_AUTH_HEADER",
                "GENYCASTER_SNAPCHAIN_AUTH_VALUE",
                "GENYCASTER_SNAPCHAIN_WRITE_AUTH_HEADER",
                "GENYCASTER_SNAPCHAIN_WRITE_AUTH_VALUE"):
        env.pop(key, None)
    env.update({
        "GENYCASTER_READ_PROVIDER_PROFILES_JSON": json.dumps([{
            "id": READ_ID,
            "label": "Personal Snapchain read node",
            "contract": "snapchain-v1",
            "endpoint": reader,
            "selfHosted": True,
        }], separators=(",", ":")),
        "GENYCASTER_WRITE_PROVIDER_PROFILES_JSON": json.dumps([{
            "id": WRITE_ID,
            "label": "Independent validator",
            "contract": "snapchain-v1",
            "endpoint": writer,
            "readEndpoint": reader,
            "selfHosted": writer_self_hosted,
        }], separators=(",", ":")),
        "GENYCASTER_PREFERRED_READ_PROVIDER_ID": READ_ID,
        "GENYCASTER_PREFERRED_WRITE_PROVIDER_ID": WRITE_ID,
        "GENYCASTER_ALLOW_COMPATIBLE_READ_FALLBACK": "0",
        # Authoritative signer GET goes to read; signed writes use the writer.
        "GENYCASTER_SNAPCHAIN_BASE_URL": reader,
        "GENYCASTER_SNAPCHAIN_WRITE_BASE_URL": writer,
    })
    return env


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--read-url", required=True,
                        help="Personally operated raw Snapchain HTTPS origin or loopback")
    parser.add_argument("--write-url", required=True,
                        help="Verified write-capable Snapchain validator origin")
    parser.add_argument("--client", default="farcaster_client",
                        help="Installed native Genycaster client executable")
    parser.add_argument("--fid", type=int,
                        help="Known FID with public casts for pre-launch no-key probe")
    parser.add_argument("--writer-self-hosted", action="store_true",
                        help="Assert the independent validator is self-hosted")
    parser.add_argument("--skip-probe", action="store_true",
                        help="Allow offline startup; does not prove node readiness")
    parser.add_argument("--dry-run", action="store_true",
                        help="Validate and print only non-secret route details; do not launch")
    args, passthrough = parser.parse_known_args()
    try:
        env = make_environment(args.read_url, args.write_url, os.environ,
                               args.writer_self_hosted)
        if args.dry_run:
            print(json.dumps({
                "mode": "strict self-hosted raw-read",
                "read": validated_origin(args.read_url),
                "write": validated_origin(args.write_url),
                "managed_read_fallback": False,
                "note": "Raw following-feed aggregation and search remain unsupported."
            }))
            return 0
        if not args.skip_probe:
            if not args.fid or args.fid <= 0:
                raise ValueError("--fid is required for an actual read-node probe")
            report = inspect_node(args.read_url, args.fid, 9, 10.0, check_graph=True)
            if report.get("result") != "pass":
                raise ValueError("Read-node probe did not pass")
        program = shutil.which(args.client)
        if not program:
            raise ValueError("Genycaster native executable was not found")
        print("Read and write endpoints pinned; launching native client.", flush=True)
        os.execvpe(program, [program, *passthrough], env)
    except (ValueError, OSError) as exc:
        # Avoid leaking full provider URLs and process environment to errors.
        print("Launch failed: " + (str(exc) if isinstance(exc, ValueError)
              else type(exc).__name__), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
