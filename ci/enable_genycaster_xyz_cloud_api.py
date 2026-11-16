#!/usr/bin/env python3
"""Expose the Genycaster account API on genycaster.xyz without replacing its landing page.

The HTTPS vhost already exists and has a valid certificate. Its root must
continue serving the public landing site until the WebAssembly release is
approved. This migration adds only the authenticated /api/v1/cloud/ proxy.

Dry-run is the default. --apply must run as an administrator on the Nginx host,
not through a Remote Operations connection with a read-only /etc mount.
"""
from __future__ import annotations

import argparse
import difflib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

DEFAULT_CONFIG = Path('/etc/nginx/sites-available/genycaster.xyz')
HOST = 'server_name genycaster.xyz;'
CERT = 'ssl_certificate /etc/letsencrypt/live/genycaster.xyz/fullchain.pem;'
ROOT = 'root /var/www/genycaster.xyz/public;'
MARKER = '    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem;\n\n'
PROXY = '''    # Genycaster native/browser account, membership, Streak and cloud API.
    # Do not proxy login or all static routes; retain the existing landing.
    # Qt native clients use the canonical Origin https://genycaster.xyz.
    location ^~ /api/v1/cloud/ {
        proxy_pass http://127.0.0.1:8797;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header X-Request-ID $request_id;
        proxy_connect_timeout 3s;
        proxy_read_timeout 30s;
        proxy_redirect off;
        client_max_body_size 208k;
        add_header Cache-Control "no-store" always;
        add_header X-Content-Type-Options "nosniff" always;
    }

'''


def revised_config(source: str) -> str:
    if (source.count(HOST) != 2 or source.count(CERT) != 1
            or source.count(ROOT) != 2 or source.count(MARKER) != 1
            or source.count('location = / {') != 1):
        raise ValueError('not the reviewed genycaster.xyz landing HTTPS vhost')
    if 'location ^~ /api/v1/cloud/' in source or 'proxy_pass http://127.0.0.1:8797;' in source:
        raise ValueError('Account API proxy already present; inspect it manually')
    return source.replace(MARKER, MARKER + PROXY, 1)


def command(*args: str) -> None:
    subprocess.run(args, check=True, timeout=25)


def probe(path: str, expected: str, *, method: str = 'GET',
          origin: str = '', data: str = '') -> None:
    args = [
        'curl', '--noproxy', '*', '--resolve', 'genycaster.xyz:443:127.0.0.1',
        '--max-time', '12', '-sS', '-o', '/dev/null', '-w', '%{http_code}',
        '-X', method,
    ]
    if origin:
        args += ['-H', f'Origin: {origin}', '-H', 'Content-Type: application/json']
    if data:
        args += ['--data', data]
    args += ['https://genycaster.xyz' + path]
    result = subprocess.run(args, check=True, text=True,
                            capture_output=True, timeout=16)
    if result.stdout.strip() != expected:
        raise RuntimeError(f'{path}: HTTPS {result.stdout.strip()}, expected {expected}')


def check_public_routes() -> None:
    # Validates that both the Nginx proxy and API's same-origin authority
    # have migrated; a 403 for the canonical Origin is a failed deployment.
    probe('/api/v1/cloud/pro/plan', '200')
    probe('/api/v1/cloud/session', '401')
    probe('/api/v1/cloud/session/challenge', '200', method='POST',
          origin='https://genycaster.xyz', data='{"fid":"4294967296"}')
    probe('/', '200')


def activate(config: Path, revised: str) -> None:
    backup = config.with_name(config.name + '.before-cloud-api-xyz-20261116')
    if backup.exists():
        raise RuntimeError(f'refusing to overwrite existing backup: {backup}')
    shutil.copy2(config, backup)
    staged: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode='w', encoding='utf-8', dir=config.parent,
            prefix=config.name + '.', suffix='.tmp', delete=False
        ) as handle:
            handle.write(revised)
            staged = Path(handle.name)
        shutil.copymode(config, staged)
        staged.replace(config)
        command('nginx', '-t')
        command('systemctl', 'reload', 'nginx')
        check_public_routes()
    except BaseException:
        if staged and staged.exists():
            staged.unlink()
        shutil.copy2(backup, config)
        command('nginx', '-t')
        command('systemctl', 'reload', 'nginx')
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    try:
        current = args.config.read_text(encoding='utf-8')
        revised = revised_config(current)
        if args.apply:
            activate(args.config, revised)
            print('genycaster.xyz Account API active; landing page preserved.')
        else:
            sys.stdout.writelines(difflib.unified_diff(
                current.splitlines(keepends=True),
                revised.splitlines(keepends=True),
                fromfile=str(args.config),
                tofile=str(args.config) + ' (proposed)',
            ))
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired, RuntimeError) as error:
        print(f'Genycaster Account API migration not applied: {error}',
              file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
