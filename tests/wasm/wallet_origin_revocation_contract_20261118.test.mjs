import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const bridge = readFileSync('src/providers/compatibility/browser_wallet/bridge.cpp','utf8');
const integration = readFileSync('tests/providers/browser_wallet/bridge_contract_tests.cpp','utf8');

test('EIP-2255 successful revocation removes both remembered identity and account grant',()=>{
  const result = bridge.slice(bridge.indexOf('void receiveResult('),bridge.indexOf('void receiveEvent('));
  assert.match(result,/if \(pending\.request\.method == "wallet_revokePermissions"\)/);
  assert.match(result,/m_accountAccessGrants\.erase\(pending\.origin\)/);
  assert.match(result,/m_grantedAccountByOrigin\.erase\(pending\.origin\)/);
  assert.match(result,/if \(authorizedReceipt\(\)\)/);
  assert.ok(result.indexOf('if (pending.request.method == "wallet_revokePermissions")')
    > result.indexOf('if (const auto* result = object.if_contains("result"))'),
    'do not revoke on an error response');
});

test('chain and principal changes invalidate wallet authorization for exact origin',()=>{
  const event = bridge.slice(bridge.indexOf('void receiveEvent('),bridge.indexOf('template <class Body>'));
  assert.match(event,/EthereumWalletEventKind::ChainChanged/);
  assert.match(event,/bool principalChanged = disconnected \|\| chainChanged/);
  assert.match(event,/m_accountAccessGrants\.clear\(\)/);
  assert.match(event,/m_grantedAccountByOrigin\.clear\(\)/);
  assert.match(event,/\+\+m_authorizationEpoch/);
  assert.match(event,/m_pending\.clear\(\)/);
  assert.match(bridge,/m_accountAccessGrants\.contains\(m_origin\)/);
  assert.match(bridge,/request\.method == "eth_accounts" \|\| request\.method == "wallet_getPermissions"/);
});

test('native C++ bridge regression executes explicit revoke, reconnection and changed-chain denials',()=>{
  assert.match(integration,/void testOriginGrantRevocationAndChainChangedRequireFreshConsent\(\)/);
  assert.match(integration,/rpc\(202U, "wallet_revokePermissions"/);
  assert.match(integration,/rpc\(203U, "eth_accounts"/);
  assert.match(integration,/rpc\(204U, "personal_sign"/);
  assert.match(integration,/rpc\(205U, "eth_requestAccounts"/);
  assert.match(integration,/rpc\(206U, "eth_accounts"/);
  assert.match(integration,/rpc\(207U, "eth_sendTransaction"/);
  assert.match(integration,/testOriginGrantRevocationAndChainChangedRequireFreshConsent\(\);/);
});


test('pending RPCs are scoped to their immutable session and authorization epochs',()=>{
  const pending = bridge.slice(bridge.indexOf('struct Pending final'),bridge.indexOf('class Bridge final'));
  assert.match(pending,/std::uint64_t authorizationEpoch\{\}/);
  assert.match(pending,/std::uint64_t sessionGeneration\{\}/);
  assert.match(bridge,/\.authorizationEpoch = m_authorizationEpoch/);
  assert.match(bridge,/\.sessionGeneration = m_sessionGeneration/);
  assert.match(bridge,/pending\.authorizationEpoch == m_authorizationEpoch/);
  assert.match(bridge,/pending\.sessionGeneration == m_sessionGeneration/);
  assert.match(bridge,/m_pending\.erase\(it\)/);
  assert.match(bridge,/m_pending\.clear\(\)/);
});

test('pending signer callbacks are rejected on revoke/chain change without holding bridge locks',()=>{
  const onResult = bridge.slice(bridge.indexOf('void receiveResult('),bridge.indexOf('void receiveEvent('));
  assert.match(onResult,/bool validReceipt = false/);
  assert.match(onResult,/if \(!validReceipt\) \{/);
  assert.match(onResult,/Wallet authorization was revoked before completion/);
  assert.match(onResult,/if \(pending\.completion\) pending\.completion\(std::move\(response\)\)/);
  const onEvent = bridge.slice(bridge.indexOf('void receiveEvent('),bridge.indexOf('template <class Body>'));
  assert.match(onEvent,/Wallet account or chain changed before completion/);
  assert.match(integration,/void testRevocationAndChainEventsCancelOutstandingRpc\(\)/);
  assert.match(integration,/signature\.get\(\)\.error->code == 4100/);
  assert.match(integration,/postResult\(signatureTransport, "0xlate-signature"\)\.result\(\) == http::status::gone/);
  assert.match(integration,/reconnect\.get\(\)\.error->code == 4100/);
});
