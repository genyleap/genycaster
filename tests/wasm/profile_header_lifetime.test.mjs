import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const composition = readFileSync(new URL('../../ui/mobile/runtime_composition.cpp', import.meta.url), 'utf8');
const bindings = readFileSync(new URL('../../ui/mobile/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
const bridge = readFileSync(new URL('../../src/presentation/qt/presentation_bridge.cpp', import.meta.url), 'utf8');
const profileQml = readFileSync(new URL('../../ui/components/ProfileSurface.qml', import.meta.url), 'utf8');

// Deterministic cancellation-order fixture coupled to the actual Qt composition
// decision points. A native Qt signal/metadata-cache regression complements it.
class ProfileNavigation {
  constructor() { this.active = ''; this.header = null; this.feed = null; this.generation = 0; }
  cancelAll() {
    this.generation++;
    if (this.header) this.header.cancelled = true;
    if (this.feed) this.feed.cancelled = true;
    this.header = this.feed = null;
    this.active = '';
  }
  open(fid) {
    this.cancelAll();
    this.header = {fid, cancelled: false, generation: this.generation, value: null};
    this.startFeed('user-posts', fid);
    return this.header;
  }
  startFeed(kind, fid) {
    const id = kind + ':' + fid;
    if (this.active !== id) {
      if (kind.startsWith('user-')) {
        if (this.feed) this.feed.cancelled = true;
      } else {
        this.cancelAll();
      }
      this.active = id;
    } else if (this.feed) this.feed.cancelled = true;
    this.feed = {fid, kind, cancelled: false};
  }
  deliverHeader(request, data) {
    if (request.cancelled || this.header !== request || request.generation !== this.generation) return false;
    request.value = data;
    return true;
  }
}

test('native composition explicitly preserves primary metadata while switching profile feed', () => {
  assert.match(composition, /if \(preserveProfileRequest\) \{[\s\S]*?cancelFeedRequest\(\);[\s\S]*?cancelReferencedPreviewRequests\(\);[\s\S]*?cancelViewerReactionRequest\(\);[\s\S]*?\} else \{[\s\S]*?cancelAllRequests\(\)/);
  assert.match(bindings, /const bool profileFeed = feed\.startsWith\(QStringLiteral\("user-"\)\)/);
  assert.match(bindings, /selectFeedSurfaceIdentity\(surfaceIdentity, profileFeed\)/);
  assert.match(bridge, /emit profileRequested\(m_profileFid, recoveryProbe\);[\s\S]*?emit feedRequested\(m_profileFeed, m_profileFid/);
  assert.match(profileQml, /visible: Number\(root\.detail\.fid \|\| 0\) > 0/);
});

test('profile metadata completes after feed starts and after multiple tab changes', () => {
  const runtime = new ProfileNavigation();
  const pending = runtime.open(101);
  assert.equal(pending.cancelled, false, 'initial feed must not cancel the header read');
  runtime.startFeed('user-likes', 101);
  runtime.startFeed('user-quotes', 101);
  runtime.startFeed('user-replies', 101);
  assert.equal(pending.cancelled, false, 'tab changes cannot cancel header read');
  assert.equal(runtime.deliverHeader(pending, {name:'Alice', followerCount:4381, followingCount:129}), true);
  assert.equal(runtime.header.value.followerCount, 4381);
});

test('switching profiles, leaving to Explorer, and stale metadata arrivals remain isolated', () => {
  const runtime = new ProfileNavigation();
  const old = runtime.open(101);
  const current = runtime.open(202);
  assert.equal(old.cancelled, true);
  assert.equal(runtime.deliverHeader(old, {name:'Old'}), false);
  assert.equal(runtime.deliverHeader(current, {name:'New', followerCount:0}), true);
  runtime.startFeed('recent', 0);
  assert.equal(current.cancelled, true);
  assert.equal(runtime.deliverHeader(current, {name:'Late'}), false);
});
