"""Account API must remain reachable without publishing the WASM prelaunch host."""
from __future__ import annotations

import unittest

from ci.repair_prelaunch_cloud_nginx import (
    ORIGINAL, REPLACEMENT, PROXY_LOCATION, PROXY_TARGET, revised_config,
)


def fake_vhost(hold: str = ORIGINAL, proxy: str = PROXY_LOCATION) -> str:
    return (
        'server {\n'
        '    listen 443 ssl;\n'
        '    server_name caster.genyleap.com;\n'
        + hold + '\n'
        '    location = / { try_files /farcaster_client.html =503; }\n'
        '    ' + proxy + '\n'
        '        ' + PROXY_TARGET + '\n'
        '    }\n'
        '}\n'
    )


class PrelaunchCloudGateTests(unittest.TestCase):
    def test_preserves_web_hold_but_exempts_only_cloud_prefix(self) -> None:
        updated = revised_config(fake_vhost())
        self.assertIn('if ($uri !~ "^/api/v1/cloud/") {\n        return 503;\n    }',
                      updated)
        self.assertIn(PROXY_LOCATION, updated)
        self.assertIn(PROXY_TARGET, updated)
        self.assertNotIn(ORIGINAL, updated)
        self.assertEqual(updated.count('server_name caster.genyleap.com;'), 1)

    def test_fail_closed_without_exact_hold(self) -> None:
        with self.assertRaises(ValueError):
            revised_config(fake_vhost('    return 503;'))

    def test_fail_closed_with_unreviewed_proxy_or_extra_copy(self) -> None:
        with self.assertRaises(ValueError):
            revised_config(fake_vhost().replace(PROXY_TARGET,
                                                'proxy_pass http://127.0.0.1:9999;'))
        with self.assertRaises(ValueError):
            revised_config(fake_vhost() + fake_vhost())

    def test_does_not_accidentally_enable_other_hosts(self) -> None:
        with self.assertRaises(ValueError):
            revised_config(fake_vhost().replace('caster.genyleap.com;',
                                                'other.example.org;'))


if __name__ == '__main__':
    unittest.main()
