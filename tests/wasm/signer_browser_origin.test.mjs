import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import { webcrypto } from 'node:crypto';
import { createBrowserSignerVault } from '../../ui/wasm/secure_signer_vault.mjs';

const source = readFileSync(
    new URL('../../ui/wasm/protocol-signer-pre.js', import.meta.url), 'utf8');

function runPreJs(origin) {
    let redirected = '';
    const page = new URL(origin);
    const location = {
        href: page.href,
        hostname: page.hostname,
        origin: page.origin,
        protocol: page.protocol,
        replace(next) { redirected = next; },
    };
    const window = {
        location, isSecureContext: page.hostname === '127.0.0.1' ||
            page.hostname === 'localhost' || page.protocol === 'https:',
        crypto: webcrypto,
        indexedDB: { open() {} },
    };
    vm.runInNewContext(source, { window, URL, console });
    return { window, redirected };
}

test('emrun 0.0.0.0 origin redirects to secure loopback, preserving path', () => {
    const result = runPreJs('http://0.0.0.0:30000/farcaster_client.html?x=1#context');
    assert.equal(result.redirected,
        'http://127.0.0.1:30000/farcaster_client.html?x=1#context');
    assert.equal(result.window.__genycasterBrowserSigning, undefined);
});

test('loopback secure origin does not redirect and exposes safe environment diagnostics', () => {
    const result = runPreJs('http://127.0.0.1:30000/farcaster_client.html');
    assert.equal(result.redirected, '');
    assert.equal(result.window.__genycasterSignerEnvironment.secureContext, true);
    assert.equal(result.window.__genycasterSignerEnvironment.webCrypto, true);
    assert.equal(result.window.__genycasterSignerEnvironment.indexedDB, true);
    assert.equal(result.window.__genycasterSignerEnvironment.origin,
        'http://127.0.0.1:30000');
    assert.equal(typeof result.window.__genycasterBrowserSigning.start, 'function');
});

test('non-loopback insecure HTTP must not be silently sent to the client computer', () => {
    const result = runPreJs('http://192.168.1.20:30000/farcaster_client.html');
    assert.equal(result.redirected, '');
    assert.equal(result.window.__genycasterSignerEnvironment.secureContext, false);
});

test('vault reports the exact insecure-origin reason instead of blaming signing', () => {
    assert.throws(() => createBrowserSignerVault({
        location: { hostname: '0.0.0.0' },
        isSecureContext: false,
        crypto: webcrypto,
        indexedDB: { open() {} },
    }), /127\.0\.0\.1/);
    assert.throws(() => createBrowserSignerVault({
        location: { hostname: '192.168.1.20' },
        isSecureContext: false,
        crypto: webcrypto,
        indexedDB: { open() {} },
    }), /Use HTTPS/);
});

test('missing WebCrypto and IndexedDB have distinct actionable errors', () => {
    assert.throws(() => createBrowserSignerVault({
        isSecureContext: true,
        indexedDB: { open() {} },
    }), /WebCrypto SubtleCrypto/);
    assert.throws(() => createBrowserSignerVault({
        isSecureContext: true,
        crypto: webcrypto,
    }), /IndexedDB is unavailable/);
    assert.throws(() => createBrowserSignerVault({
        isSecureContext: true,
        crypto: webcrypto,
        get indexedDB() { throw new DOMException('blocked', 'SecurityError'); },
    }), /IndexedDB is unavailable/);
});
