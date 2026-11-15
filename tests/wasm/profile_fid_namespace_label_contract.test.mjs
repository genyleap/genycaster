import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = (path) => readFileSync(new URL('../../' + path, import.meta.url), 'utf8');

test('public profiles cannot be classified from the 2^32 FID allocation boundary', () => {
  const qml = read('ui/components/profile/ProfileSurface.qml');
  assert.doesNotMatch(qml, /4294967296|Math\\.pow\\(2,\\s*32\\)/);
  assert.doesNotMatch(qml, /activeDetail\\.fid[^\\n]*>=/);
  // Native branding is a property of the verified signed-in account only.
  assert.match(qml, /root\\.isOwnProfile\\s*&&\\s*root\\.accountIsNativeCustody/);
  assert.match(qml, /qsTrId\\("genycaster\\.settings\\.hypersnap_label"\\)/);
  // Unknown public identity must remain neutral, not falsely say Farcaster.
  assert.match(qml, /qsTrId\\("genycaster\\.side_nav\\.profile"\\)/);
  assert.doesNotMatch(qml, /qsTrId\\("genycaster\\.profile\\.farcaster_profile"\\)/);
});

test('native registration still validates the native FID lower bound at its trusted boundary', () => {
  const session = read('src/application/session.cpp');
  assert.match(session, /constexpr std::uint64_t FirstNativeFid = \\(1ULL << 32\\)/);
  assert.match(session, /session\\.fid\\.value < FirstNativeFid/);
  assert.match(session, /session\\.identityIssuer != NativeIssuer/);
  assert.match(session, /session\\.identitySubject != expectedSubject/);
  assert.match(session, /session\\.method != domain::session::AuthenticationMethod::Custody/);
});

test('generic title is translated in each shipped locale and not just the English catalog', () => {
  const locales = ['en_US','ar_SA','de_DE','es_ES','fa_IR','fr_FR',
                   'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  const expression = /<message id="genycaster\\.side_nav\\.profile">\\s*<source>Profile<\\/source>\\s*<translation>([^<]+)<\\/translation>/;
  for (const locale of locales) {
    const catalog = read('ui/i18n/qml_' + locale + '.ts');
    assert.match(catalog, expression, 'missing generic profile translation: ' + locale);
  }
});
