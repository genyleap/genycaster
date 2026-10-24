import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const root = new URL('../../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8');
const qml = read('ui/components/FeedSurface.qml');
const validA = '0x' + 'aa'.repeat(32);
const validB = '0x' + 'bb'.repeat(32);

function compileFilter(state) {
  const match = /    function rowMatchesSignerFilter\(row\) \{[\s\S]*?\n    \}\n\n    function sourceRows/.exec(qml);
  assert.ok(match, 'the actual QML filter must be present');
  const body = match[0].replace(/\n\n    function sourceRows$/, '');
  return new Function('root', 'return (' + body + ')')(state);
}

test('the signer-key filter defaults to all casts, including unknown', () => {
  const filter = compileFilter({signerFilterMode: 'all', selectedSignerKey: ''});
  assert.equal(filter({}), true);
  assert.equal(filter({sourceClient:'Popular App'}), true);
  assert.equal(filter({signerPublicKeyHex:validA}), true);
});

test('include mode requires an exact observed protocol signer, never a claimed client badge', () => {
  const selected = compileFilter({signerFilterMode:'include', selectedSignerKey:validA});
  assert.equal(selected({signerPublicKeyHex:validA}), true);
  assert.equal(selected({signerPublicKeyHex:validA.toUpperCase().replace('0X','0x')}), true);
  assert.equal(selected({signerPublicKeyHex:validB}), false);
  assert.equal(selected({sourceClient:'App A'}), false);
  assert.equal(selected({}), false);
  const missing = compileFilter({signerFilterMode:'include', selectedSignerKey:''});
  assert.equal(missing({}), false);
  assert.equal(missing({signerPublicKeyHex:validA}), false);
});

test('exclude mode removes only the exact signer while keeping unknown casts', () => {
  const filter = compileFilter({signerFilterMode:'exclude', selectedSignerKey:validB});
  assert.equal(filter({signerPublicKeyHex:validB}), false);
  assert.equal(filter({signerPublicKeyHex:validA}), true);
  assert.equal(filter({sourceClient:'Untrusted App'}), true);
  assert.equal(filter({}), true);
});

test('canonical signer key is persisted, not inferred from cached display metadata', () => {
  const cast = read('src/domain/cast.cppm');
  const codec = read('src/providers/snapchain/read.cpp');
  const storage = read('src/storage/entity_store.cpp');
  const schema = read('src/storage/sqlite_boundary.cpp');
  const presenter = read('ui/mobile/runtime_ui_bindings.cpp');
  assert.match(cast, /std::optional<std::string> signerPublicKeyHex/);
  assert.match(codec, /result\.signerPublicKeyHex = canonicalSignerKey\(message\)/);
  assert.match(codec, /value->size\(\) != 44U/);
  assert.match(storage, /signer_public_key_hex = COALESCE\(excluded\.signer_public_key_hex, casts\.signer_public_key_hex\)/);
  assert.match(storage, /cast\.signerPublicKeyHex = sqlite::optionalColumnText/);
  assert.match(schema, /recordMigration\(connection, 27, appliedAt\)/);
  assert.match(presenter, /QStringLiteral\("signerPublicKeyHex"\)/);
  assert.match(read('tests/unit/read_pipeline_tests.cpp'),
    /protocolSignerProvenanceSurvivesCacheAndSchemaUpgrade\(\)/);
  assert.match(read('tests/providers/snapchain/read_contract_tests.cpp'),
    /testProtocolSignerKeyIsCanonicalButNeverGuessesAppIdentity\(\)/);
});

test('QML surfaces an explicit unverified-app warning and offers include/exclude', () => {
  assert.match(qml, /genycaster\.feed\.signer_filter_explanation/);
  assert.match(qml, /root\.rowMatchesSignerFilter\(row\)/);
  assert.match(qml, /signerFilterMode = index === 1 \? "include"/);
  assert.match(qml, /selectedSignerKey = option\.key/);
  assert.match(qml, /\+ localRecentNotice\.height \+ signerFilterBar\.height \+ appFilterBar\.height/);
  const locales = ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                   'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for (const locale of locales) {
    const translation = read('ui/i18n/qml_' + locale + '.ts');
    for (const key of ['signer_filter_explanation','signer_filter_all',
                       'signer_filter_include','signer_filter_exclude']) {
      assert.match(translation,
        new RegExp('<message id="genycaster\\.feed\\.' + key
          + '">[\\s\\S]*?<translation>[^<]+</translation>'));
    }
  }
});


function compileAppFilter(state) {
  const found = /    function rowMatchesAppFilter\(row\) \{[\s\S]*?\n    \}\n\n    function rowMatchesSignerFilter/.exec(qml);
  assert.ok(found, 'the actual QML App FID filter must exist');
  const body = found[0].replace(/\n\n    function rowMatchesSignerFilter$/, '');
  return new Function('root', 'return (' + body + ')')(state);
}

test('App FID filter includes only node-attested FID while preserving unknown in exclude mode', () => {
  const all = compileAppFilter({appFilterMode:'all', selectedAppFid:''});
  assert.equal(all({sourceAppFid:'333'}), true);
  assert.equal(all({sourceClient:'App FID 333'}), true);
  const only = compileAppFilter({appFilterMode:'include', selectedAppFid:'333'});
  assert.equal(only({sourceAppFid:'333'}), true);
  assert.equal(only({sourceAppFid:'777'}), false);
  assert.equal(only({sourceClient:'App FID 333'}), false);
  assert.equal(only({}), false);
  const without = compileAppFilter({appFilterMode:'include', selectedAppFid:''});
  assert.equal(without({sourceAppFid:'333'}), false);
  const excluding = compileAppFilter({appFilterMode:'exclude', selectedAppFid:'333'});
  assert.equal(excluding({sourceAppFid:'333'}), false);
  assert.equal(excluding({sourceAppFid:'777'}), true);
  assert.equal(excluding({sourceClient:'App FID 333'}), true);
  assert.equal(excluding({}), true);
});

test('App FID registry route and canonical cache and locale controls are wired', () => {
  const read = file => readFileSync(new URL(file, root), 'utf8');
  const decoder = read('src/providers/snapchain/read.cpp');
  const store = read('src/storage/entity_store.cpp');
  const schema = read('src/storage/sqlite_boundary.cpp');
  const bridge = read('ui/mobile/runtime_ui_bindings.cpp');
  assert.match(decoder, /decodeSignerAppFid\(/);
  assert.match(decoder, /\/v1\/signer\?fid=/);
  assert.match(decoder, /maximumLookups = 8U/);
  assert.match(decoder, /self->page\.casts/);
  assert.match(store, /source_app_fid = COALESCE/);
  assert.match(schema, /recordMigration\(connection, 28, appliedAt\)/);
  assert.match(bridge, /QStringLiteral\("sourceAppFid"\)/);
  assert.match(qml, /root\.rowMatchesAppFilter\(row\)/);
  assert.match(qml, /selectedAppFid = option\.fid/);
  assert.match(qml, /appFilterBar\.height/);
  const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
                 'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for (const locale of locales) {
    assert.match(read('ui/i18n/qml_'+locale+'.ts'),
      /<message id="genycaster\.feed\.app_fid_filter_explanation">[\s\S]*?<translation>[^<]+<\/translation>/);
  }
});

test('live gasless signer TTL uses Farcaster epoch, not Unix seconds', () => {
  const source = read('src/providers/snapchain/read.cpp');
  const fixture = read('tests/providers/snapchain/read_contract_tests.cpp');
  assert.match(source, /farcasterEpochUnixSeconds = 1'609'459'200LL/);
  assert.match(source, /unixNow - farcasterEpochUnixSeconds/);
  assert.match(source, /currentFarcasterSeconds/);
  assert.match(fixture, /unixNow - farcasterEpochUnixSeconds/);
});
