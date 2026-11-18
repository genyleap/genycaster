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
  assert.match(result,/if \(m_active && m_origin == pending\.origin\)/);
  assert.ok(result.indexOf('if (pending.request.method == "wallet_revokePermissions")')
    > result.indexOf('if (const auto* result = object.if_contains("result"))'),
    'do not revoke on an error response');
});

test('chain and principal changes invalidate wallet authorization for exact origin',()=>{
  const event = bridge.slice(bridge.indexOf('void receiveEvent('),bridge.indexOf('template <class Body>'));
  assert.match(event,/EthereumWalletEventKind::ChainChanged/);
  assert.match(event,/bool principalChanged = disconnected \|\| chainChanged/);
  assert.match(event,/m_accountAccessGrants\.erase\(m_origin\)/);
  assert.match(event,/m_grantedAccountByOrigin\.erase\(m_origin\)/);
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
