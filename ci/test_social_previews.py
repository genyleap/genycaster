"""Tests for current GenyCaster OG and Twitter share metadata."""
from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
import unittest

from ci.build_social_cards import CARDS, png_dimensions, render_source, measure, TEXT_FONT
from ci.deploy_social_previews import META, SRC, LIVE, with_tags, collect

ROOT = Path(__file__).resolve().parent.parent


class HeadMetadata(HTMLParser):
    def __init__(self):
        super().__init__()
        self.metatags: dict[str,list[str]] = {}
        self.canonical: list[str] = []

    def handle_starttag(self, tag, attrs):
        props = dict(attrs)
        if tag == "meta":
            name = props.get("property") or props.get("name")
            if name:
                self.metatags.setdefault(name, []).append(props.get("content", ""))
        if tag == "link" and props.get("rel") == "canonical":
            self.canonical.append(props.get("href", ""))


class SocialPreviewTests(unittest.TestCase):
    def test_home_and_download_have_single_correct_social_metadata(self):
        inputs = (
            (SRC / "genycaster-shell.html", "home"),
            (SRC / "download/index.html", "downloads"),
        )
        for source, channel in inputs:
            with self.subTest(source=str(source)):
                value = source.read_text(encoding="utf-8")
                parsed = HeadMetadata()
                parsed.feed(value)
                metadata = META[channel]
                for key, expected in (
                    ("og:title", metadata["title"]),
                    ("og:url", metadata["url"]),
                    ("og:image", metadata["image"]),
                    ("og:image:alt", metadata["alt"]),
                    ("twitter:title", metadata["title"]),
                    ("twitter:image", metadata["image"]),
                    ("twitter:card", "summary_large_image"),
                    ("og:image:width", "1200"),
                    ("og:image:height", "630"),
                    ("description", metadata["description"]),
                ):
                    self.assertEqual(parsed.metatags.get(key), [expected], key)
                self.assertEqual(parsed.canonical, [metadata["url"]])
                self.assertEqual(value, with_tags(value, channel), "idempotence")

    def test_relay_about_head_matches_download_for_non_js_scrapers(self):
        about = LIVE / "about/index.html"
        if not about.is_file():
            self.skipTest("Production server only")
        html = about.read_text(encoding="utf-8")
        parsed = HeadMetadata()
        parsed.feed(html)
        self.assertEqual(parsed.metatags.get("og:url"),
                         ["https://genycaster.xyz/download"])
        self.assertEqual(parsed.metatags.get("og:image"),
                         [META["downloads"]["image"]])
        self.assertIn('/download/about-forward.js?v=3', html)

    def test_fallback_index_has_current_home_preview(self):
        # A second public HTML document previously retained the 2025-style
        # "Your Farcaster" image even after WASM itself was updated.
        source = LIVE / "index.html"
        if not source.is_file():
            self.skipTest("Production server only")
        contents = source.read_text(encoding="utf-8")
        current = with_tags(contents, "home")
        parsed = HeadMetadata()
        parsed.feed(current)
        self.assertEqual(parsed.metatags.get("og:image"),
                         [META["home"]["image"]])
        self.assertNotIn("og-genycaster-v5.png", current)
        self.assertEqual(current, with_tags(current, "home"))

    def test_raster_text_is_measured_and_not_rendered_through_svg_delegate(self):
        # The old ImageMagick SVG font delegate overlapped glyphs on the PNG.
        source = (ROOT / "ci/build_social_cards.py").read_text()
        self.assertIn('background = re.sub', source)
        self.assertIn('if b"<text " in background:', source)
        self.assertIn('"-annotate"', source)
        self.assertIn('width * 1.3 > limit', source)
        width, height = measure("Download GenyCaster", TEXT_FONT, 57)
        self.assertGreater(width, 500)
        self.assertLess(width, 800)
        self.assertGreater(height, 50)

    def test_png_cards_use_correct_dimensions_and_expected_messages(self):
        for name, detail in CARDS.items():
            with self.subTest(name=name):
                file = SRC / (name + ".png")
                self.assertEqual(png_dimensions(file.read_bytes()[:24]),
                                 (1200, 630))
                svg = render_source(detail).decode("utf-8")
                self.assertIn("#829AFF", svg)
                self.assertIn("#0B0C0F", svg)
                self.assertIn('width="1200" height="630"', svg)
                self.assertIn(detail["headline"], svg)
                self.assertNotIn("Coming soon", svg)
                self.assertNotIn("IOS", detail["footer"])

    def test_future_wasm_bundles_keep_social_previews(self):
        packer = (ROOT / "ci/package_wasm_assets.py").read_text()
        for name in CARDS:
            self.assertEqual(packer.count(f'"{name}.png"'), 2)
        self.assertEqual(len(collect()), 8)


if __name__ == "__main__":
    unittest.main()
