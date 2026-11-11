#!/usr/bin/env python3
"""Guard Linux distribution branding and one-line profile link layout."""
from __future__ import annotations

import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONTROLLER = ROOT / "src/presentation/qt/appearance_controller.cpp"
ICON_ROOT = ROOT / "ui/assets/icons/genyleap/icons/platforms"
PROFILE = ROOT / "ui/components/profile/ProfileSurface.qml"

BRAND = re.compile(
    r'\{"(?P<id>[a-z0-9-]+)", "(?P<name>[^"]+)", '
    r'"(?P<color>#[0-9A-Fa-f]{6})", "platforms/(?P<icon>[a-z0-9-]+)"\}'
)


def distro_entries() -> dict[str, tuple[str, str, str]]:
    source = CONTROLLER.read_text(encoding="utf-8")
    mapping = source.split("constexpr LinuxBrand kLinuxBrands[] = {", 1)[1].split("};", 1)[0]
    entries = BRAND.findall(mapping)
    if len(entries) != len({distro for distro, _, _, _ in entries}):
        raise ValueError("Duplicate /etc/os-release distribution ID in Linux theme mapping")
    return {distro: (name, accent, icon) for distro, name, accent, icon in entries}


class LinuxDistroBrandAssetsTest(unittest.TestCase):
    def test_all_mapped_distros_have_real_vector_assets(self):
        entries = distro_entries()
        self.assertGreaterEqual(len(entries), 30)
        for distro, (_name, _accent, icon) in entries.items():
            with self.subTest(distro=distro):
                svg_path = ICON_ROOT / f"{icon}.svg"
                self.assertTrue(svg_path.is_file(), svg_path)
                node = ET.parse(svg_path).getroot()
                self.assertTrue(node.tag.endswith("svg"))
                self.assertEqual(node.attrib.get("viewBox"), "0 0 24 24")
                self.assertTrue(
                    any(child.tag.endswith("path") for child in node.iter()),
                    "A real vector path is required rather than a placeholder",
                )
                self.assertEqual(node.attrib.get("fill"), "#000000")

    def test_names_and_icons_match_os_release_ids(self):
        entries = distro_entries()
        for distro, expected_icon in (
            ("ubuntu", "ubuntu"),
            ("debian", "debian"),
            ("fedora", "fedora"),
            ("arch", "archlinux"),
            ("linuxmint", "linuxmint"),
            ("pop", "popos"),
            ("kali", "kalilinux"),
            ("rhel", "redhat"),
            ("neon", "kdeneon"),
            ("nixos", "nixos"),
            ("asahi", "asahilinux"),
        ):
            self.assertEqual(entries[distro][2], expected_icon)

    def test_unknown_distro_fails_to_generic_linux_not_ubuntu(self):
        source = CONTROLLER.read_text(encoding="utf-8")
        self.assertIn(
            'kGenericLinuxBrand{"linux", "Linux", "#3584E4", "platforms/linux"}',
            source,
        )
        self.assertIn("return kGenericLinuxBrand;", source)
        self.assertIn("QSysInfo::productType()", source)
        self.assertNotIn("QSysInfo::productName()", source)

    def test_profile_inline_url_never_wraps_or_elides(self):
        qml = PROFILE.read_text(encoding="utf-8")
        start = qml.index('id: identityPrimaryLink')
        end = qml.index('id: identityMoreLinks', start)
        link = qml[start:end]
        self.assertIn("wrapMode: Text.NoWrap", link)
        self.assertIn("elide: Text.ElideNone", link)
        self.assertIn("profileLinkMetrics.advanceWidth", link)
        self.assertNotIn("Text.WrapAnywhere", link)
        self.assertIn("clip: false", link)
        self.assertIn("Layout.minimumWidth: Layout.preferredWidth", link)
        self.assertIn("Layout.minimumWidth: Math.min(fullLinkWidth, root.profileContentWidth)", qml)
        self.assertIn("columns: root.profileContentWidth >=", qml)
        self.assertNotIn("root.profileContentWidth * 0.76", qml)
        self.assertIn("Layout.minimumWidth: implicitWidth", qml[end:end + 500])


if __name__ == "__main__":
    unittest.main()
