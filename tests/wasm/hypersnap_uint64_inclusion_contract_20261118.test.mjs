import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const transport = readFileSync('ui/app/identity/hypersnap_registration_transport.cpp', 'utf8');
const nativeTests = readFileSync('tests/custody_recovery/hypersnap_transport.cpp', 'utf8');

test('HyperSnap native FID is parsed as canonical decimal uint64, never JSON double', () => {
  assert.match(transport, /std::pair<bool, std::uint64_t> exactUnsigned\(const QJsonValue& value\)/);
  assert.match(transport, /if \(!value\.isString\(\)\)/);
  assert.match(transport, /text\.size\(\) > 1 && text\.front\(\) == QLatin1Char\('0'\)/);
  assert.match(transport, /text\.toULongLong\(&ok, 10\)/);
  assert.match(transport, /fid < FirstNativeFid/);
  assert.match(nativeTests, /9007199254740993ULL/);
  assert.match(nativeTests, /04294967296/);
});

test('shard-0 inclusion heights reject fractional and unsafe IEEE-754 or overflow values', () => {
  assert.match(transport, /exactInclusionHeight\(const QJsonValue& value\)/);
  assert.match(transport, /if \(value\.isString\(\)\)\s*return exactUnsigned\(value\)/);
  assert.match(transport, /MaxSafeJsonInteger = 9007199254740991\.0/);
  assert.match(transport, /!std::isfinite\(height\)/);
  assert.match(transport, /std::trunc\(height\) != height/);
  assert.match(transport, /!blockValid \|\| blockNumber == 0/);
  assert.doesNotMatch(transport, /static_cast<std::uint64_t>\(block\.toDouble\(\)\)/);
  assert.match(nativeTests, /54321\.5/);
  assert.match(nativeTests, /9007199254740992/);
  assert.match(nativeTests, /18446744073709551616/);
});

test('a hub lookup never turns a verified wallet into a canonical OPP FID link', () => {
  assert.match(transport, /custody != expected/);
  assert.match(transport, /!data\.value\(QStringLiteral\("message"\)\)\.isObject\(\)/);
  assert.match(transport, /canonical OPP FID linking still requires its own trusted verifier/);
  assert.match(transport, /QNetworkRequest::ManualRedirectPolicy/);
});
