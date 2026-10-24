import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');

test('raw protocol and indexed continuations bind endpoint, capability, and provider', () => {
    const raw = read('src/providers/snapchain/read.cpp');
    const indexed = read('src/providers/hypersnap/continuation.cpp');
    assert.match(raw, /"raw-v2:" \+ m_config\.baseUrl/);
    assert.match(raw, /record->providerId\.value != m_config\.providerId\.value/);
    assert.match(raw, /record->capabilityId\.value != capabilityId\.value/);
    assert.match(raw, /different endpoint or user FID/);
    assert.match(indexed, /"indexed-v3:" \+ std::move\(endpointIdentity\)/);
    assert.match(indexed, /std::to_string\(selectorIdentity\.size\(\)\)/);
    assert.match(indexed, /record->providerId\.value != m_providerId\.value/);
    assert.match(indexed, /record->capabilityId\.value != capabilityId\.value/);
    assert.match(indexed, /providerCursor\.starts_with\(m_endpointPrefix\)/);
    assert.match(indexed, /m_endpointPrefix \+ std::move\(\*providerCursor\)/);
});

test('all asynchronous and blocking indexed reads supply endpoint identity', () => {
    for (const name of ['async_gateway', 'feed', 'notification', 'social']) {
        const source = read('src/providers/hypersnap/' + name + '.cpp');
        const matches = [...source.matchAll(/continuation::Store\s+\w+\s*\{([^}]+)\}/g)];
        assert.ok(matches.length > 0, name + ' must use bound continuations');
        for (const match of matches) {
            assert.match(match[1], /(?:m_config|config)\.baseUrl/,
                name + ' has a continuation missing its HTTP endpoint');
        }
    }
    const contract = read('tests/providers/hypersnap/contract_tests.cpp');
    assert.match(contract, /testIndexedEndpointBoundContinuation\(\)/);
    assert.match(contract, /differentEndpoint\.resolve/);
    assert.match(contract, /legacy-cursor/);
    assert.match(contract, /differentFid\.resolve/);
    assert.match(contract, /differentChannel\.bind/);
    assert.match(contract, /scienceChannel\.resolve/);
    const syncFeed = read("src/providers/hypersnap/feed.cpp");
    const asyncFeed = read("src/providers/hypersnap/async_gateway.cpp");
    assert.match(syncFeed, /continuation::selectorScope\(query\)/);
    assert.match(asyncFeed, /continuation::selectorScope\(spec\.query\)/);
    assert.match(syncFeed, /"fid:" \+ std::to_string\(fid\.value\)/);
    assert.match(asyncFeed, /query\.kind == application::read::model::FeedQuery::Kind::UserLikes/);
});

test('no provider cursor enters the canonical cast or profile structures', () => {
    const cast = read('src/domain/cast.cppm');
    const profile = read('src/domain/profile.cppm');
    assert.doesNotMatch(cast + profile, /providerCursor|pageToken|endpointIdentity/);
});
