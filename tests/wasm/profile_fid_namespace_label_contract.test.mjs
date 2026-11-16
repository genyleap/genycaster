import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = (path) => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('public profiles cannot be classified from the 2^32 FID allocation boundary', () => {
  const qml = read('ui/components/profile/ProfileSurface.qml');
  assert.doesNotMatch(qml, /4294967296|Math\.pow\(2,\s*32\)/);
  const labelStart = qml.indexOf('id: profileDisplayName');
  assert.ok(labelStart >= 0, 'profile display label must exist');
  const label = qml.slice(labelStart, qml.indexOf('color: Theme.ink', labelStart));
  assert.doesNotMatch(label, /activeDetail\.fid|4294967296|>=/);
  // Native branding is a property of the verified signed-in account only.
  assert.match(qml, /root\.isOwnProfile\s*&&\s*root\.accountIsNativeCustody/);
  assert.match(qml, /qsTrId\("genycaster\.settings\.hypersnap_label"\)/);
  // Unknown public identity must remain neutral, not falsely say Farcaster.
  assert.match(qml, /qsTrId\("genycaster\.side_nav\.profile"\)/);
  assert.doesNotMatch(qml, /qsTrId\("genycaster\.profile\.farcaster_profile"\)/);
});

test('native registration still validates the native FID lower bound at its trusted boundary', () => {
  const session = read('src/application/session.cpp');
  assert.match(session, /constexpr std::uint64_t FirstNativeFid = \(1ULL << 32\)/);
  assert.match(session, /session\.fid\.value < FirstNativeFid/);
  assert.match(session, /session\.identityIssuer != NativeIssuer/);
  assert.match(session, /session\.identitySubject != expectedSubject/);
  assert.match(session, /session\.method != domain::session::AuthenticationMethod::Custody/);
});

test('generic title is translated in each shipped locale and not just the English catalog', () => {
  const locales = ['en_US','ar_SA','de_DE','es_ES','fa_IR','fr_FR',
                   'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  const expression = /<message id="genycaster\.side_nav\.profile">\s*<source>Profile<\/source>\s*<translation>([^<]+)<\/translation>/;
  for (const locale of locales) {
    const catalog = read('ui/i18n/qml_' + locale + '.ts');
    assert.match(catalog, expression, 'missing generic profile translation: ' + locale);
  }
});

test('unset native usernames are never rendered as fake profile handles', () => {
  const decoder = read('src/providers/hypersnap/codec.cpp');
  const presenter = read('src/presentation/qt/presentation_bridge.cpp');
  const profile = read('ui/components/profile/ProfileSurface.qml');
  const card = read('ui/components/profile/ProfileTrustCard.qml');
  const sidebar = read('ui/components/profile/SidebarSummary.qml');
  const home = read('ui/components/home/HomeDashboard.qml');

  assert.ok(decoder.includes('handle.starts_with("fid:")'));
  assert.ok(presenter.includes('visibleProfileUsername(std::move(username))'));
  assert.ok(presenter.includes('m_accountUsername = nativeCustodyAccount() ? QString{}'));
  assert.ok(presenter.includes('return nativeCustodyAccount() ? QString{} : m_accountUsername;'));
  assert.ok(profile.includes('visible: root.showUsername'));
  const ownershipStart = profile.indexOf('readonly property bool isOwnProfile:');
  const ownershipEnd = profile.indexOf('readonly property int profileAvatarSize', ownershipStart);
  assert.ok(ownershipStart > 0 && ownershipEnd > ownershipStart);
  assert.ok(!profile.slice(ownershipStart, ownershipEnd).includes('username'),
    'the edit-profile affordance must depend on authenticated FID, not a username');
  assert.ok(profile.includes('objectName: "profileEditAction"'));
  assert.ok(card.includes('shownUsername: /^fid:'));
  assert.ok(!card.includes('root.username : "genycaster"'));
  assert.ok(sidebar.includes('root.authenticated && root.shownUsername.trim().length > 0'));
  assert.ok(home.includes('welcomeName.length === 0'));
  assert.ok(!home.includes('root.username || qsTrId("genycaster.side_nav.genycaster")'));
});
