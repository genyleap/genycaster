import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const file = path => readFileSync(new URL('../../'+path, import.meta.url),'utf8');
const browser = file('ui/app/browser_openproof_auth.cpp');
const provider = file('ui/app/provider_profiles_controller.cpp');

function functionBody(source, start, end) {
  const begin=source.indexOf(start);
  const finish=source.indexOf(end, begin+start.length);
  assert.ok(begin>=0 && finish>begin, 'Method boundaries found for '+start);
  return source.slice(begin,finish);
}
function assertReentrancySafe(body) {
  const detach=body.indexOf('m_reply.clear()')>=0
    ? body.indexOf('m_reply.clear()') : body.indexOf('m_probeReply.clear()');
  const disconnect=body.indexOf('QObject::disconnect(pending.data(), nullptr, this, nullptr)');
  const abort=body.indexOf('pending->abort()');
  const destroy=body.indexOf('if (pending) pending->deleteLater()');
  assert.ok(detach>=0 && disconnect>detach && abort>disconnect && destroy>abort,
    'QPointer detached, callback disconnected and guarded before/after abort');
}

test('OpenProof browser authorization cannot dereference a cleared reply when abort delivers finished inline',()=>{
  const body=functionBody(browser,
    'void BrowserOpenProofAuth::cancelPendingReply()',
    'void BrowserOpenProofAuth::reset()');
  assert.match(body,/QPointer<QNetworkReply> pending = m_reply/);
  assertReentrancySafe(body);
  assert.match(functionBody(browser,'void BrowserOpenProofAuth::reset()','void BrowserOpenProofAuth::emitState'),
    /cancelPendingReply\(\)/);
  assert.match(functionBody(browser,'void BrowserOpenProofAuth::send(','void BrowserOpenProofAuth::begin('),
    /cancelPendingReply\(\)/);
});
test('OpenProof browser cancellation retains generation guards and does not retain the previous reply',()=>{
  assert.match(browser,/\+\+m_generation/);
  assert.match(browser,/if \(generation != m_generation\)/);
  const body=functionBody(browser,'void BrowserOpenProofAuth::send(','void BrowserOpenProofAuth::begin(');
  assert.doesNotMatch(body,/m_reply->abort\(\)|m_reply->deleteLater\(\)/);
});
test('provider endpoint probes also detach previous reply before aborting',()=>{
  const body=functionBody(provider,
    'void ProviderProfilesController::cancelProbeReply()',
    'bool ProviderProfilesController::startProbe(');
  assert.match(body,/QPointer<QNetworkReply> pending = m_probeReply/);
  assertReentrancySafe(body);
  const start=functionBody(provider,'bool ProviderProfilesController::startProbe(',
    'void ProviderProfilesController::runProbeStep(');
  assert.equal((start.match(/cancelProbeReply\(\)/g)||[]).length,2,
    'both the invalid and the replacement probe cancel safely');
  const clear=functionBody(provider,
    'void ProviderProfilesController::clearDraftProbe()',
    'QString ProviderProfilesController::activeReadContract()');
  assert.match(clear,/cancelProbeReply\(\)/);
  assert.doesNotMatch(start,/m_probeReply->abort\(\)/);
});

test('cancelled network replies are never read after QIODevice closes',()=>{
  assert.match(browser,/reply->isReadable\(\) \? reply->readAll\(\) : QByteArray\{\}/);
  assert.match(provider,/reply->isReadable\(\) \? reply->read\(48 \* 1024\) : QByteArray\{\}/);
});

test('channel discovery and updates never drain closed QNetworkReply devices',()=>{
  const channel=file('ui/app/channel_picker_controller.cpp');
  const updater=file('ui/app/update_controller.cpp');
  for(const [source,expected] of [[channel,2],[updater,5]]){
    const reads=(source.match(/reply->readAll\(\)/g)||[]).length;
    const readable=(source.match(/reply->isReadable\(\) \? reply->readAll\(\)/g)||[]).length;
    assert.equal(reads,expected,'known number of response reads is explicit');
    assert.equal(readable,reads,'all reads guard against cancelled/closed devices');
  }
  assert.match(updater,/m_hash\.addData\(rest\)/);
  assert.match(updater,/m_hash\.addData\(bytes\)/);
  assert.match(updater,/m_file->write\(rest\)/);
});
