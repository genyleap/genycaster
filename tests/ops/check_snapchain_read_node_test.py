#!/usr/bin/env python3
"""No-live-network acceptance tests for the independent Snapchain node probe."""
import importlib.util
import json
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
import unittest
from urllib.error import HTTPError

SCRIPT = Path(__file__).resolve().parents[2] / "ci" / "check_snapchain_read_node.py"
spec = importlib.util.spec_from_file_location("snapchain_probe", SCRIPT)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class MockSnapchain(BaseHTTPRequestHandler):
    seen = []
    info = {"shardInfos": [{"shardId": "0", "maxHeight": "1000", "blockDelay": "3"},
                           {"shardId": "1", "maxHeight": "1100", "blockDelay": "2"}]}
    casts = {"messages": [{"data": {"castAddBody": {"text": "hello"}}}]}
    redirect = False

    def log_message(self, *args):
        pass

    def do_GET(self):
        type(self).seen.append((self.path, self.headers.get("Authorization"),
                                self.headers.get("x-api-key"), self.headers.get("Cookie")))
        if type(self).redirect:
            self.send_response(302)
            self.send_header("Location", "https://example.com/v1/info")
            self.end_headers()
            return
        if self.path == "/v1/info":
            response = type(self).info
        elif self.path == "/v1/castsByFid?fid=123&pageSize=10":
            response = type(self).casts
        elif self.path == "/v1/linksByFid?fid=123&link_type=follow&pageSize=10":
            response = type(self).following
        elif self.path == "/v1/linksByTargetFid?target_fid=123&link_type=follow&pageSize=10":
            response = type(self).followers
        else:
            self.send_error(404)
            return
        payload = json.dumps(response).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


class ProbeTests(unittest.TestCase):
    def setUp(self):
        MockSnapchain.seen = []
        MockSnapchain.redirect = False
        MockSnapchain.info = {"shardInfos": [
            {"maxHeight": "1000", "blockDelay": "3"},
            {"maxHeight": "1100", "blockDelay": "2"}]}
        MockSnapchain.casts = {"messages": [
            {"data": {"castAddBody": {"text": "hello"}}}]}
        MockSnapchain.following = {"messages": [
            {"data": {"fid": 123, "linkBody": {"type": "follow", "targetFid": 7}}}]}
        MockSnapchain.followers = {"messages": [
            {"data": {"fid": 7, "linkBody": {"type": "follow", "targetFid": 123}}}]}
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), MockSnapchain)
        self.thread = Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = f"http://127.0.0.1:{self.server.server_address[1]}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def test_real_http_read_uses_no_external_api_credentials(self):
        result = probe.run(self.url, fid=123, max_delay=9, timeout=3)
        self.assertEqual(result["result"], "pass")
        self.assertEqual(result["shards"], 2)
        self.assertEqual(result["cast_messages"], 1)
        self.assertEqual(len(MockSnapchain.seen), 2)
        self.assertTrue(all(not any(row[1:]) for row in MockSnapchain.seen))

    def test_graph_endpoints_without_api_key(self):
        result = probe.run(self.url, fid=123, max_delay=9, timeout=3,
                           check_graph=True)
        self.assertIn("raw_follow_graph_without_api_key", result["checked"])
        self.assertEqual(result["following_links"], 1)
        self.assertEqual(result["follower_links"], 1)
        self.assertEqual(len(MockSnapchain.seen), 4)
        self.assertTrue(all(not any(row[1:]) for row in MockSnapchain.seen))

    def test_wrong_link_fid_fails_closed(self):
        MockSnapchain.following = {"messages": [
            {"data": {"fid": 77, "linkBody": {"type": "follow", "targetFid": 7}}}]}
        with self.assertRaisesRegex(ValueError, "wrong FID"):
            probe.run(self.url, fid=123, max_delay=9, timeout=3, check_graph=True)

    def test_graph_missing_data_fails_closed(self):
        MockSnapchain.followers = {"error": "unimplemented"}
        with self.assertRaisesRegex(ValueError, "no messages array"):
            probe.run(self.url, fid=123, max_delay=9, timeout=3, check_graph=True)

    def test_stale_shard_fails_closed(self):
        MockSnapchain.info["shardInfos"][1]["blockDelay"] = "99"
        with self.assertRaisesRegex(ValueError, "unsynced shard"):
            probe.run(self.url, fid=123, max_delay=9, timeout=3)

    def test_empty_casts_do_not_count_as_read_proof(self):
        MockSnapchain.casts = {"messages": []}
        with self.assertRaisesRegex(ValueError, "no cast messages"):
            probe.run(self.url, fid=123, max_delay=9, timeout=3)

    def test_redirect_refused_instead_of_using_other_host(self):
        MockSnapchain.redirect = True
        with self.assertRaises(HTTPError):
            probe.run(self.url, fid=123, max_delay=9, timeout=3)
        self.assertEqual(len(MockSnapchain.seen), 1)

    def test_remote_http_and_url_secrets_rejected(self):
        for url in ("http://node.example.com:3381", "https://a:b@node.example",
                    "https://node.example?a=1", "https://node.example/path",
                    "ftp://node.example"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                probe.validated_origin(url)

    def test_incomplete_sync_info_never_passes(self):
        for bad in ({}, {"shardInfos": []},
                    {"shardInfos": [{"maxHeight": "10"}]},
                    {"shardInfos": [{"maxHeight": "0", "blockDelay": "1"}]}):
            with self.subTest(info=bad), self.assertRaises(ValueError):
                probe.verify_info(bad, 9)


if __name__ == "__main__":
    unittest.main()
