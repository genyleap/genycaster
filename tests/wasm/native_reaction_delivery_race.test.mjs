import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

// Deterministic fake event loop for the asynchronous native reaction ownership
// contract. This fixture also pins its decision points to the actual C++ path;
// it is not a substitute for an instrumented compiled Qt runtime interleaving.
const binding = readFileSync(new URL('../../ui/mobile/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
const runtime = readFileSync(new URL('../../ui/mobile/runtime_composition.cpp', import.meta.url), 'utf8');
const definition = readFileSync(new URL('../../ui/mobile/runtime_composition.cppm', import.meta.url), 'utf8');

class DeliveryFixture {
  constructor() {
    this.account = 3; this.surface = 1; this.nextGeneration = 0;
    this.delivered = new Map(); this.fresh = new Map(); this.current = new Map();
    this.gui = []; this.now = 0;
  }
  complete(evidence, examined, account = this.account) {
    const generation = ++this.nextGeneration;
    const surface = this.surface;
    // Worker completion queues a GUI job; it MUST NOT mark any target fresh.
    this.gui.push(() => {
      if (account !== this.account || surface !== this.surface) return false;
      for (const [castId, fields] of Object.entries(evidence)) {
        const row = {...(this.current.get(castId) ?? {})};
        for (const [role, value] of Object.entries(fields)) {
          const key = role + ':' + castId;
          if ((this.delivered.get(key) ?? 0) > generation) continue;
          this.delivered.set(key, generation);
          this.fresh.set(key, this.now + 90);
          row[role] = value;
        }
        this.current.set(castId, row);
      }
      for (const [castId, roles] of Object.entries(examined)) {
        for (const role of roles) {
          const key = role + ':' + castId;
          if ((this.fresh.get(key) ?? 0) <= this.now) this.fresh.set(key, this.now + 15);
        }
      }
      return true;
    });
    return generation;
  }
  navigate() { ++this.surface; }
  changeAccount(next) { this.account = next; ++this.surface; this.delivered.clear(); this.fresh.clear(); }
}

test('native source splits surface lifetime from batch generation and commits freshness on GUI', () => {
  assert.match(definition, /viewerSurfaceGeneration/);
  assert.match(definition, /viewerReactionDeliveredGeneration/);
  assert.match(runtime, /\+\+viewerSurfaceGeneration/);
  const gui = binding.slice(binding.indexOf('QMetaObject::invokeMethod(\n                targetBridge,'));
  assert.match(gui, /surfaceGeneration != runtime->viewerSurfaceGeneration/);
  assert.doesNotMatch(gui.slice(0, gui.indexOf('Qt::QueuedConnection);')), /generation != runtime->viewerOperationGeneration/);
  assert.match(gui, /viewerReactionFreshUntil\.insert_or_assign/);
  assert.match(gui, /lastDelivered > generation/);
  assert.match(gui, /existing->second <= now/);
  assert.match(binding, /obsoleteRequest->cancel\(\);/);
});

test('A completes, B starts on the same surface, then A GUI delivery is accepted', () => {
  const fixture = new DeliveryFixture();
  fixture.complete({a: {liked: true}}, {a: ['liked']}); // A worker done; GUI waiting.
  fixture.complete({b: {recasted: true}}, {b: ['recasted']}); // B starts.
  assert.equal(fixture.fresh.has('liked:a'), false);
  assert.equal(fixture.gui.shift()(), true);
  assert.equal(fixture.current.get('a').liked, true);
  assert.equal(fixture.fresh.get('liked:a'), 90);
  assert.equal(fixture.gui.shift()(), true);
  assert.equal(fixture.current.get('b').recasted, true);
});

test('newer confirmed field survives older queued evidence while independent recast survives', () => {
  const f = new DeliveryFixture();
  f.complete({a: {liked: false, recasted: true}}, {a: ['liked', 'recasted']});
  f.complete({a: {liked: true}}, {a: ['liked']});
  assert.equal(f.gui.pop()(), true); // B arrives first
  assert.equal(f.gui.pop()(), true); // late A
  assert.deepEqual(f.current.get('a'), {liked: true, recasted: true});
});

test('navigation and account changes reject obsolete pending deliveries without freshness', () => {
  const f = new DeliveryFixture();
  f.complete({a: {liked: true}}, {a: ['liked']});
  f.navigate();
  assert.equal(f.gui.shift()(), false);
  assert.equal(f.current.has('a'), false);
  assert.equal(f.fresh.has('liked:a'), false);
  f.complete({b: {liked: true}}, {b: ['liked']});
  f.changeAccount(9);
  assert.equal(f.gui.shift()(), false);
  assert.equal(f.fresh.has('liked:b'), false);
});

test('partial unknown refreshes expired cooldown instead of preserving a dead entry', () => {
  const f = new DeliveryFixture();
  f.complete({}, {a: ['liked']}); assert.equal(f.gui.shift()(), true);
  assert.equal(f.fresh.get('liked:a'), 15);
  f.now = 17;
  f.complete({}, {a: ['liked']}); assert.equal(f.gui.shift()(), true);
  assert.equal(f.fresh.get('liked:a'), 32);
});


test('native viewport interests are retained and released by the same pooled EngagementBar', () => {
  const bar = readFileSync(new URL('../../ui/components/EngagementBar.qml', import.meta.url), 'utf8');
  const header = readFileSync(new URL('../../src/presentation/qt/presentation_bridge.hpp', import.meta.url), 'utf8');
  assert.match(bar, /Qt\.platform\.os !== "wasm"/);
  assert.match(bar, /syncNativeInterest\(\)/);
  assert.match(bar, /onNativeInterestKeyChanged: syncNativeInterest\(\)/);
  assert.match(bar, /onEnrichmentActiveChanged:[\s\S]*?syncNativeInterest\(\)/);
  assert.match(bar, /onScrollMovingChanged:[\s\S]*?syncNativeInterest\(\)/);
  assert.match(bar, /Component\.onDestruction:[\s\S]*?releaseViewerReactionInterest/);
  assert.match(header, /viewerReactionInterestRetained/);
  assert.match(header, /viewerReactionInterestReleased/);
  assert.match(binding, /viewerReactionConsumerCounts\.contains\(target\.id\.value\)/);
  assert.match(binding, /std::erase_if\(runtime->deferredViewerTargets,/);
});

test('reference-counted viewport demand shares work and drains more than a 64-target batch', () => {
  // A bounded miniature of the native queue contract. Assertions against the
  // actual C++ admission limits keep this model aligned with production.
  assert.match(binding, /maximumBatch\{64U\}/);
  assert.match(binding, /deferredViewerTargets\.size\(\) < 512U/);
  const counts = new Map(); const waiting = []; const batches = [];
  const retain = id => {
    const count = counts.get(id) || 0; counts.set(id, count + 1);
    if (count === 0) waiting.push(id);
  };
  const release = id => {
    const n = counts.get(id) || 0;
    if (n <= 1) { counts.delete(id); const pos = waiting.indexOf(id); if (pos >= 0) waiting.splice(pos, 1); }
    else counts.set(id, n - 1);
  };
  retain('shared'); retain('shared');
  assert.equal(counts.get('shared'), 2);
  release('shared'); assert.equal(counts.get('shared'), 1);
  for (let i = 0; i < 130; i++) retain('cast-'+i);
  release('cast-0'); // no transport admission for a recycled delegate
  while (waiting.length) {
    const batch = waiting.splice(0, 64).filter(key => counts.has(key));
    if (batch.length) batches.push(batch);
  }
  assert.equal(batches.length, 3);
  assert.equal(batches.flat().length, 130); // 129 distinct + one shared
  assert.equal(new Set(batches.flat()).size, 130);
  assert.ok(batches.every(batch => batch.length <= 64));
});
