"""Offline regression coverage for the Snapchain Explorer freshness diagnostic."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import pathlib
import unittest
from unittest.mock import patch


ROOT = pathlib.Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "tools" / "probe_farcaster_event_freshness.py"
SPEC = importlib.util.spec_from_file_location("probe_farcaster_event_freshness", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


def cast_event(hash_suffix, timestamp, parent=False):
    cast_body = {"text": "private synthetic cast"}
    if parent:
        cast_body["parentCastId"] = {"fid": 4, "hash": "0x" + "a" * 40}
    return {
        "type": "HUB_EVENT_TYPE_MERGE_MESSAGE",
        "mergeMessageBody": {"message": {
            "hash": "0x" + hash_suffix * 40,
            "data": {
                "type": "MESSAGE_TYPE_CAST_ADD",
                "timestamp": timestamp,
                "castAddBody": cast_body,
            },
        }},
    }


class FreshnessProbeTests(unittest.TestCase):
    def test_root_casts_across_data_shards_are_hydrated_without_exposing_content(self):
        calls = []
        def fake_fetch(base_url, path, timeout):
            calls.append(path)
            if path == "/v1/info":
                return {"shardInfos": [
                    {"shardId": 0, "maxHeight": 1000},
                    {"shardId": 1, "maxHeight": 2000},
                    {"shardId": 2, "maxHeight": 3000},
                ]}
            if path.startswith("/v1/events"):
                if "shard_index=1" in path:
                    return {"events": [cast_event("a", 123), cast_event("b", 124, True)]}
                if "shard_index=2" in path:
                    return {"events": [cast_event("c", 125)]}
            if path.startswith("/v2/farcaster/cast/bulk?"):
                return {"casts": [{"hash": "0x" + "a" * 40},
                                  {"hash": "0x" + "c" * 40}]}
            raise AssertionError(path)

        output = io.StringIO()
        with patch.object(module, "fetch_json", side_effect=fake_fetch):
            with contextlib.redirect_stdout(output):
                code = module.probe("https://example.invalid", 40, 2.0, True)
        self.assertEqual(code, 0)
        self.assertIn("total_root_casts=2", output.getvalue())
        self.assertIn("requested_hydration=2 hydrated_casts=2", output.getvalue())
        self.assertNotIn("private synthetic cast", output.getvalue())
        self.assertEqual(len([p for p in calls if p.startswith("/v1/events")]), 2)
        self.assertFalse(any("shard_index=0" in path for path in calls))

    def test_returns_failure_when_all_shards_are_empty(self):
        def fake_fetch(_base, path, _timeout):
            return {"shardInfos": [{"shardId": 1, "maxHeight": 120}]} \
                if path == "/v1/info" else {"events": []}
        with patch.object(module, "fetch_json", side_effect=fake_fetch):
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(module.probe("https://example.invalid", 40, 2.0), 1)


if __name__ == "__main__":
    unittest.main()
