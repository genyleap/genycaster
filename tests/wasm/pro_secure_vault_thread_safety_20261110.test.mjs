import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = p => readFileSync(p, 'utf8');
const controller = read('ui/app/identity/connections.cpp');
const header = read('ui/app/identity/connections.hpp');
const keychain = read('src/platform/signer_vault_qtkeychain.cpp');

test('native secure-vault account proof never blocks the GUI/network callback', () => {
  assert.match(keychain, /void runJob\(QKeychain::Job& job\)[\s\S]*?loop\.exec\(QEventLoop::ExcludeUserInputEvents\)/);
  const code = controller.split('void OpenProofConnectionsController::beginSignedAccountSession()')[1]
    .split('void OpenProofConnectionsController::restoreAccountSession()')[0];
  assert.match(code, /QtConcurrent::run\(\[signer = m_accountProofSigner, viewer, nonce\]/);
  assert.match(code, /auto\* watcher = new QFutureWatcher<void>\(this\)/);
  assert.match(code, /watcher->setFuture\(\*signedFuture\)/);
  assert.match(code, /QtConcurrent::run\([\s\S]*?return signer\(viewer, nonce\)/);
  assert.match(code, /serial != m_accountProofSignSerial/);
  assert.match(code, /generation != m_grantGeneration/);
  assert.match(code, /watcher->deleteLater\(\)/);
  assert.match(code, /m_accountProofSigning = false;[\s\S]*?submitProof\(signedFuture->result\(\)\)/);
  assert.doesNotMatch(code.split('#else')[1], /proof = m_accountProofSigner\(viewer, nonce\)/);
});

test('pending asynchronous proof remains busy and is invalidated on account changes', () => {
  assert.match(header, /m_accountProofRequest != nullptr \|\| m_accountProofSigning/);
  assert.match(header, /std::uint64_t m_accountProofSignSerial\{\}/);
  assert.match(controller, /void OpenProofConnectionsController::cancelAccountSessionRequest\(\)[\s\S]*?\+\+m_accountProofSignSerial/);
  assert.match(controller, /void OpenProofConnectionsController::clearAccountGrant\(\)[\s\S]*?\+\+m_accountProofSignSerial/);
  assert.match(controller, /if \(m_sessionRequest \|\| m_accountProofRequest \|\| m_accountProofSigning\)/);
  assert.match(controller, /m_sessionRevoking \|\| m_sessionRequest \|\| m_accountProofSigning/);
  assert.match(controller, /QTimer::singleShot\(20'000, this, \[this, serial, viewer, generation\]/);
  assert.match(controller, /m_accountProofError = QStringLiteral\("account_signer_unavailable"\)/);
});

test('Android signer and account proof jobs finish on the same QtKeychain thread', () => {
  assert.match(keychain, /QThreadPool& keychainWorker\(\)[\s\S]*?setMaxThreadCount\(1\);\s*setExpiryTimeout\(-1\);[\s\S]*?static Worker worker;/);
  assert.match(keychain, /if \(!worker.contains\(QThread::currentThread\(\)\)\)[\s\S]*?worker.start\(std::move\(task\)\);\s*return result.get\(\);/);
  for (const type of ['Read', 'Write', 'Delete'])
    assert.ok(keychain.includes('withKeychainJob<QKeychain::' + type + 'PasswordJob>'));
});
