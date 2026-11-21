"""The native website catalog must match exactly the currently published assets."""
from __future__ import annotations
import unittest
from ci import sync_native_release_catalog as catalog
from ci.refresh_existing_release import EXPECTED_ASSETS

class DownloadCatalogTests(unittest.TestCase):
    def setUp(self):
        self.digests={name:format(i+1,'064x') for i,name in
                      enumerate(sorted(EXPECTED_ASSETS-{"SHA256SUMS"}))}
        self.source="".join(f"{d}  {f}\n" for f,d in self.digests.items())

    def test_all_seven_official_installer_urls(self):
        self.assertEqual(catalog.parse_checksums(self.source),self.digests)
        data=catalog.generate(self.digests)
        self.assertEqual(data["schema"],1)
        self.assertEqual(sum(len(p) for p in data["platforms"].values()),7)
        for (platform,arch),filename in catalog.INSTALLERS.items():
            record=data["platforms"][platform][arch]
            self.assertEqual(record["url"],catalog.PREFIX+filename)
            self.assertEqual(record["sha256"],self.digests[filename])
            self.assertEqual(record["version"],"1.0.0")
            self.assertEqual(record.get("distribution"),
                             "ad-hoc-unnotarized" if platform=="macos" else None)

    def test_missing_duplicate_changed_assets_fail_closed(self):
        lines=self.source.splitlines()
        for src in ("\n".join(lines[1:]),self.source+self.source.splitlines()[0]+"\n",
                    self.source.replace("GenyCaster-v1.0.0-", "EvilCaster-v1.0.0-",1),
                    self.source.replace(lines[0][:64],"invalid-checksum",1)):
            with self.subTest(source=src[:80]):
                with self.assertRaises(ValueError):
                    catalog.parse_checksums(src)

    def test_incompatible_partial_manifests_never_escape(self):
        shortened=self.digests.copy()
        shortened.pop(next(iter(shortened)))
        with self.assertRaises(ValueError):catalog.generate(shortened)

    def test_wasm_publisher_preserves_release_catalog_not_empty_template(self):
        from pathlib import Path
        server=Path("/opt/genycaster/caster-deployment/publish-canonical-wasm-20261119.py")
        if not server.is_file():self.skipTest("Not deployed on production VPS")
        text=server.read_text(encoding="utf-8")
        self.assertIn('if name == "releases/native.json":',text)
        self.assertIn("continue",text.split('if name == "releases/native.json":',1)[1][:110])

if __name__=="__main__":
    unittest.main()
