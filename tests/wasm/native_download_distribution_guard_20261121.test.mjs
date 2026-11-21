import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = path => readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('macOS unnotarized distribution metadata survives catalog validation and is disclosed', () => {
    const js=read('ui/wasm/native-downloads.js');
    const catalog=read('ci/generate_native_release_catalog.py');
    assert.match(catalog,/record\["distribution"\] = \(/);
    assert.match(catalog,/ad-hoc-unnotarized/);
    assert.match(js,/record\.distribution === "ad-hoc-unnotarized"/);
    assert.match(js,/record\.distribution === "developer-id-notarized"/);
    assert.match(js,/return \{url: url\.href, version: record\.version, sha256: record\.sha256,\s+distribution\}/);
    assert.match(js,/entry\.distribution === "ad-hoc-unnotarized"/);
    assert.match(js,/NOT notarized by Apple/);
    assert.match(js,/macOS Gatekeeper may block the download/);
});
