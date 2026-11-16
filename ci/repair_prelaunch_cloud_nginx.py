#!/usr/bin/env python3
"""Safely allow the existing account API while keeping unpublished WASM disabled.

The 2026-11-15 Nginx pre-launch hold used a server-level `return 503`.
A server-level return also intercepts *every* location, including the existing
authenticated /api/v1/cloud/ proxy. Never remove the hold without replacing it.
Run from the actual host with privileged Nginx configuration access; Remote Ops
may intentionally mount /etc read-only. No secrets are printed or changed.
"""
from __future__ import annotations

import argparse
import difflib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

DEFAULT_CONFIG = Path("/etc/nginx/sites-available/caster.genyleap.com")
ORIGINAL = (
    '    # PRE-LAUNCH HOLD 2026-11-15: unpublished host disabled.\n'
    '    add_header Cache-Control "no-store, max-age=0" always;\n'
    '    return 503;'
)
REPLACEMENT = (
    '    # PRE-LAUNCH HOLD: the unpublished WASM frontend stays disabled.\n'
    '    # Allow only the existing first-party account API for native clients.\n'
    '    # The URI is normalized by Nginx; all non-API paths still return 503.\n'
    '    add_header Cache-Control "no-store, max-age=0" always;\n'
    '    if ($uri !~ "^/api/v1/cloud/") {\n'
    '        return 503;\n'
    '    }'
)
PROXY_LOCATION = 'location ^~ /api/v1/cloud/ {'
PROXY_TARGET = 'proxy_pass http://127.0.0.1:8797;'


def revised_config(original: str) -> str:
    """Fail closed on unexpected/changed reverse-proxy configurations."""
    if 'server_name caster.genyleap.com;' not in original:
        raise ValueError('not the reviewed caster.genyleap.com vhost')
    if original.count(PROXY_LOCATION) != 1 or original.count(PROXY_TARGET) != 1:
        raise ValueError('missing or duplicated first-party cloud proxy')
    if ORIGINAL not in original or original.count(ORIGINAL) != 1:
        raise ValueError('expected prelaunch hold is missing or already changed')
    return original.replace(ORIGINAL, REPLACEMENT, 1)


def check_return(args: list[str]) -> None:
    subprocess.run(args, check=True, timeout=20)


def probe_public_route(route: str, expected: str) -> None:
    # Probe THIS Nginx vhost rather than a CDN edge; preserve TLS hostname
    # validation, and do not send any cookies or account credentials.
    result = subprocess.run([
        'curl', '--noproxy', '*', '--resolve',
        'caster.genyleap.com:443:127.0.0.1', '--max-time', '9',
        '-sS', '-o', '/dev/null', '-w', '%{http_code}',
        'https://caster.genyleap.com' + route,
    ], capture_output=True, text=True, timeout=12, check=True)
    if result.stdout.strip() != expected:
        raise RuntimeError(
            f'wrong HTTPS status for {route}: {result.stdout.strip()} '
            f'(expected {expected})'
        )


def activate(config: Path, content: str) -> None:
    """Back up, validate Nginx, and roll back automatically on any failure."""
    backup = config.with_name(config.name + '.before-cloud-api-20261116')
    if backup.exists():
        raise RuntimeError(f'backup already exists: {backup}')
    shutil.copy2(config, backup)
    original_mode = config.stat().st_mode & 0o777
    try:
        with tempfile.NamedTemporaryFile(
            mode='w', encoding='utf-8', dir=config.parent,
            prefix=config.name + '.', suffix='.tmp', delete=False,
        ) as tmp:
            tmp.write(content)
            staged = Path(tmp.name)
        staged.chmod(original_mode)
        staged.replace(config)
        check_return(['nginx', '-t'])
        check_return(['systemctl', 'reload', 'nginx'])
        probe_public_route('/api/v1/cloud/pro/plan', '200')
        probe_public_route('/api/v1/cloud/session', '401')
        probe_public_route('/', '503')
        probe_public_route('/farcaster_client.html', '503')
    except Exception:
        shutil.copy2(backup, config)
        check_return(['nginx', '-t'])
        check_return(['systemctl', 'reload', 'nginx'])
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--apply', action='store_true',
                        help='modify Nginx vhost, with backup/test/rollback')
    args = parser.parse_args()
    try:
        original = args.config.read_text(encoding='utf-8')
        updated = revised_config(original)
        if args.apply:
            activate(args.config, updated)
            print('Account API Nginx gate installed; WASM prelaunch hold remains.')
            print('Check HTTPS GET /api/v1/cloud/pro/plan (200), / (503),')
            print('and unauthenticated /api/v1/cloud/session (401).')
        else:
            sys.stdout.writelines(difflib.unified_diff(
                original.splitlines(keepends=True),
                updated.splitlines(keepends=True),
                fromfile=str(args.config), tofile=str(args.config) + ' (proposed)'))
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f'Prelaunch Account API gate not installed: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
