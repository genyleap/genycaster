"""Do not break the existing Genycaster landing page while moving the account API."""
from __future__ import annotations

import unittest
from ci.enable_genycaster_xyz_cloud_api import (
    CERT, HOST, MARKER, PROXY, ROOT, revised_config,
)


def sample() -> str:
    return (
        'server {\n'
        '    listen 80;\n'
        '    ' + HOST + '\n'
        '    ' + ROOT + '\n'
        '    location / { return 301 https://genycaster.xyz$request_uri; }\n'
        '}\n'
        'server {\n'
        '    listen 443 ssl;\n'
        '    ' + HOST + '\n'
        '    ' + ROOT + '\n'
        '    ' + CERT + '\n'
        + MARKER
        + '    location = / { try_files /index.html =404; }\n'
        '    location / { try_files $uri $uri/ =404; }\n'
        '}\n'
    )


class GenycasterDomainApiTests(unittest.TestCase):
    def test_adds_only_account_proxy_without_changing_landing(self) -> None:
        previous = sample()
        actual = revised_config(previous)
        self.assertIn(PROXY, actual)
        self.assertIn('proxy_pass http://127.0.0.1:8797;', actual)
        self.assertEqual(actual.count('location ^~ /api/v1/cloud/'), 1)
        self.assertEqual(actual.count('location = / { try_files /index.html =404; }'), 1)
        self.assertEqual(actual.count('location / { try_files $uri $uri/ =404; }'), 1)
        self.assertEqual(actual.count('ssl_certificate /etc/letsencrypt/live/genycaster.xyz/'), 1)
        self.assertNotIn('Access-Control-Allow-Origin', actual)
        self.assertNotIn('return 503;', actual)

    def test_refuses_other_sites_and_duplicate_proxy(self) -> None:
        with self.assertRaises(ValueError):
            revised_config(sample().replace('genycaster.xyz;', 'evil.example.org;'))
        with self.assertRaises(ValueError):
            revised_config(sample().replace(ROOT, 'root /somewhere-else;', 1))
        with self.assertRaises(ValueError):
            revised_config(sample().replace(MARKER, MARKER+PROXY))

    def test_rejects_uncertified_or_unreviewed_templates(self) -> None:
        with self.assertRaises(ValueError):
            revised_config(sample().replace(CERT, ''))
        with self.assertRaises(ValueError):
            revised_config(sample().replace(MARKER, ''))
        with self.assertRaises(ValueError):
            revised_config(sample().replace('location = / {', 'location = /broken {'))


if __name__ == '__main__':
    unittest.main()
