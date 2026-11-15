"""Regression checks for the executable-source provider boundary."""
import unittest

from ci.source_comments import without_comments


class SourceCommentTests(unittest.TestCase):
    def test_line_and_block_comments_are_ignored(self):
        source = "// hypersnap\n/* client.farcaster.xyz */\nint native = 42;"
        masked = without_comments(source)
        self.assertNotIn("hypersnap", masked)
        self.assertNotIn("client.farcaster.xyz", masked)
        self.assertIn("int native = 42;", masked)
        self.assertEqual(len(masked), len(source))
        self.assertEqual(masked.count("\n"), source.count("\n"))

    def test_ordinary_strings_urls_and_issuer_literals_remain(self):
        source = 'auto endpoint = "https://api.farcaster.xyz/path";\n'
        source += 'auto issuer = "hypersnap:native-registry";'
        self.assertEqual(without_comments(source), source)

    def test_strings_with_comment_markers_cannot_hide_a_later_comment(self):
        source = 'auto s = "/* not a comment */"; // hypersnap\n'
        source += 'auto t = "https://ws.farcaster.xyz";'
        masked = without_comments(source)
        self.assertIn('"/* not a comment */"', masked)
        self.assertNotIn("// hypersnap", masked)
        self.assertIn('"https://ws.farcaster.xyz"', masked)

    def test_raw_cpp_literals_are_not_stripped(self):
        source = 'auto s = R"tag(// hypersnap /* url */)tag"; // fake provider\n'
        masked = without_comments(source)
        self.assertIn('// hypersnap /* url */', masked)
        self.assertNotIn("// fake provider", masked)

    def test_javascript_templates_preserve_provider_names(self):
        source = 'const issuer = `hypersnap:native-registry`; /* remark */\n'
        masked = without_comments(source)
        self.assertIn('hypersnap:native-registry', masked)
        self.assertNotIn('remark', masked)


if __name__ == "__main__":
    unittest.main()
