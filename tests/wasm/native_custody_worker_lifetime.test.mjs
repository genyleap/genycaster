import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

// A global Qt pool may outlive a QObject controller. Workers must not capture
// Qt instances, UI state, or signer-vault storage by reference. UI completion
// belongs to a receiver-owned watcher and is revoked on account changes.
test('native OpenProof proof signing has QObject-owned completion and generation guards',()=>{
  const text=source('ui/app/identity/connections.cpp');
  assert.match(text,/QtConcurrent::run\(\[signer = m_accountProofSigner, viewer, nonce\]/);
  assert.match(text,/new QFutureWatcher<void>\(this\)/);
  assert.match(text,/serial != m_accountProofSignSerial/);
  assert.match(text,/generation != m_grantGeneration/);
  assert.match(text,/QTimer::singleShot\(20'000, this/);
});

test('native HyperSnap proof-of-work is cancellable without capturing its controller',()=>{
  const text=source('ui/app/identity/native_prepare.cpp');
  assert.match(text,/NativeRegistrationPreparationController::~NativeRegistrationPreparationController\(\)/);
  assert.match(text,/~NativeRegistrationPreparationController\(\)[\s\S]*?cancel\(\);/);
  assert.match(text,/QtConcurrent::run\(\s*\[cancellation, address, anchor, difficulty\]/);
  assert.match(text,/cancellation->load\(std::memory_order_relaxed\)/);
  assert.match(text,/m_watcher\.setFuture\(/);
});

test('native recovery phrase workers retain only zeroizing value-owned buffers',()=>{
  const text=source('ui/app/identity/native_wallet.cpp');
  const captures=[...text.matchAll(/QtConcurrent::run\(\s*\[([^\]]*)\]/g)];
  assert.ok(captures.length>=4,'signing functions must remain on cancellable async futures');
  assert.ok(captures.every(([,capture])=>!/(?:\bthis\b|&)/.test(capture)),
    'no worker may capture the destroyed controller or a stack reference');
  assert.match(text,/bytes->fill\(char\{0\}\)/);
  assert.match(text,/std::shared_ptr<QByteArray>/);
  assert.match(text,/privateWords = std::move\(privateWords\)/);
  assert.match(text,/signOpenProofFarcasterChallenge/);
  assert.match(text,/signOpenProofWalletChallenge/);
});
