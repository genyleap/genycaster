import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const pro=read('ui/components/profile/GenyProDetailsDrawer.qml');
const checkout=read('ui/app/identity/pro_checkout.cpp');
const checkoutHeader=read('ui/app/identity/pro_checkout.hpp');
const profile=read('ui/components/profile/ProfileSurface.qml');
const shell=read('ui/components/shell/AppShell.qml');
const header=read('ui/app/identity/connections.hpp');
const controller=read('ui/app/identity/connections.cpp');
const cloud=read('services/account-sync/cloud_state.mjs');
const store=read('services/account-sync/pg_store.mjs');
const migration=read('services/account-sync/migrations/014_genybuzz.sql');

test('Continue advances automatically when the quote succeeds, without auto-paying',()=>{
  assert.match(pro,/function openQuotedCheckout\(\)/);
  assert.match(pro,/onPaymentQuoteChanged:[\s\S]*?root\.openQuotedCheckout\(\)/);
  const stage=pro.slice(pro.indexOf('    function openQuotedCheckout()'),
    pro.indexOf('    function prepareQuote()'));
  assert.match(stage,/root\.checkoutOpen = true/);
  assert.match(stage,/root\.displayedQuoteId/);
  assert.doesNotMatch(stage,/proCheckout\.pay\(/,
    'a network quote response must not authorize a wallet transaction');
  const proceed=pro.slice(pro.indexOf('    function proceed()'),
    pro.indexOf('    function primaryLabel()'));
  assert.match(proceed,/root\.openQuotedCheckout\(\)/);
  const beforePayment=proceed.slice(0,proceed.indexOf('if (root.txSubmitted)'));
  assert.equal(beforePayment.includes('proCheckout.pay('),false,
    'pressing Continue must first show the quote rather than auto-pay');
  assert.match(pro,/visible: root\.quoteAttempted && !root\.checkoutActive/);
  assert.match(pro,/objectName: "genyProWalletConnectError"/);
  assert.match(pro,/function quoteErrorMessage\(\)/);
  assert.match(pro,/genycaster\.pro\.quote_rate_unavailable/);
  assert.match(pro,/genycaster\.pro\.quote_chain_unavailable/);
  assert.match(pro,/root\.purchaseIntent = false\s*\n\s*root\.lastRequestedAsset = ""/);
});

test('Continue opens a wallet selector on the user gesture, never a silent transfer',()=>{
  assert.match(checkoutHeader,/Q_INVOKABLE void connectWallet\(\)/);
  assert.match(checkoutHeader,/Q_PROPERTY\(bool walletConnected READ walletConnected NOTIFY changed\)/);
  assert.match(checkout,/void ProCheckoutController::connectWallet\(\)/);
  const first=checkout.slice(checkout.indexOf('void ProCheckoutController::connectWallet()'),
    checkout.indexOf('void ProCheckoutController::pay()'));
  assert.match(first,/m_begin\(\)/);
  assert.match(first,/request\(QStringLiteral\("eth_requestAccounts"\)/);
  assert.doesNotMatch(first,/requestChain\(\)|sendTransfer\(\)|eth_sendTransaction/);
  const pay=checkout.slice(checkout.indexOf('void ProCheckoutController::pay()'),
    checkout.indexOf('void ProCheckoutController::request('));
  assert.match(pay,/if \(alreadyConnected\)/);
  assert.match(pay,/m_quote\.value\(QStringLiteral\("verified_payers"\)\)/);
  assert.match(pay,/requestChain\(\)/);
  assert.match(pro,/proCheckout\.connectWallet\(\)/);
  assert.match(pro,/onWalletConnectedSuccessfully\(\)/);
  assert.match(pro,/proCheckout\.pay\(\)/);
  assert.match(pro,/root\.walletAttempted && !root\.checkoutActive/);
  assert.match(pro,/visible: root\.quoteAttempted && !root\.checkoutActive/);
  assert.match(pro,/qsTrId\("genycaster\.pro\.quote_wallet_required"\)/);
  assert.match(pro,/qsTrId\("genycaster\.pro\.quote_retry_notice"\)/);
});

test('Pro wallet verification signs an account-bound challenge before requesting a quote',()=>{
  const proof=read('services/account-sync/pro_wallet_proof.mjs');
  const migration15=read('services/account-sync/migrations/015_pro_wallet_proof.sql');
  const bridge=read('src/providers/compatibility/browser_wallet/bridge.cpp');
  const browserWallet=read('ui/app/identity/browser_pro_wallet.hpp');
  const icon=read('ui/wasm/genycaster-icon.svg');
  assert.match(proof,/Ethereum Signed Message/);
  assert.match(proof,/Account issuer:/);
  assert.match(proof,/verifyProWalletSignature/);
  assert.match(migration15,/consumed_at/);
  assert.match(migration15,/CREATE TABLE IF NOT EXISTS pro_wallets/);
  assert.match(header,/Q_INVOKABLE void requestProWalletChallenge/);
  assert.match(header,/Q_PROPERTY\(bool proWalletVerified/);
  assert.match(controller,/\/api\/v1\/cloud\/pro\/wallet\/challenge/);
  assert.match(controller,/\/api\/v1\/cloud\/pro\/wallet\/verify/);
  assert.match(checkout,/void ProCheckoutController::signWalletChallenge\(/);
  assert.match(checkout,/request\(QStringLiteral\("personal_sign"\)/);
  assert.match(browserWallet,/QStringLiteral\("personal_sign"\)/);
  assert.match(browserWallet,/\['eth_requestAccounts','personal_sign','eth_chainId'/);
  assert.match(pro,/onProWalletChallengeReady\(id, message\)/);
  assert.match(pro,/onProWalletVerificationCompleted\(\)/);
  assert.match(pro,/root\.walletVerified/);
  assert.match(pro,/root\.quoteAttempted[\s\S]*root\.lastRequestedAsset === root\.selectedAsset/);
  assert.match(cloud,/store\.getProWallets\(owner\)/);
  assert.match(bridge,/@media\(prefers-color-scheme:dark\)/);
  assert.match(bridge,/Signature approved\. Return to GenyCaster/);
  // The local approval page must use the G-arrow logo paths, not a Unicode
  // star or an unrelated generic brand placeholder.
  for(const [,path] of icon.matchAll(/<path fill="[^"]+" d="([^"]+)"/g))
    assert.ok(bridge.includes(path),'missing canonical brand geometry');
  assert.doesNotMatch(bridge,/brand-mark" aria-hidden="true">✦/);
});

test('GenyBuzz remains opt-in, profile-only and independent of protocol writers',()=>{
  assert.match(cloud,/handleGenyBuzz\(/);
  assert.match(migration,/CREATE TABLE IF NOT EXISTS genybuzz_preferences/);
  assert.match(migration,/enabled boolean NOT NULL DEFAULT false/);
  assert.match(migration,/CREATE TABLE IF NOT EXISTS genybuzz_events/);
  assert.match(migration,/context IN \('profile','dm'\)/);
  assert.match(store,/pg_advisory_xact_lock/);
  assert.match(store,/recipient_count/);
  assert.match(store,/sender_day_count/);
  assert.match(store,/recipient_day_count/);
  assert.match(read('services/account-sync/genybuzz.mjs'),/input.context!=='profile'/);
  assert.match(store,/muted_profiles/);
  assert.match(header,/Q_INVOKABLE void sendGenyBuzz\(qulonglong targetFid/);
  assert.match(controller,/authorizeCloudRequest\(request\)/);
  assert.match(controller,/cancelNetworkReply\(m_buzzInboxRead, this\)/);
  assert.match(profile,/objectName: "genyBuzzReceiveToggle"/);
  assert.match(profile,/objectName: "genyBuzzSendFromProfile"/);
  assert.match(profile,/sharePopup\.close\(\)\s*\n\s*root\.buzzRequested\(/);
  assert.doesNotMatch(profile,/root\.buzzError/);
  assert.match(shell,/objectName: "genyBuzzSendFeedback"/);
  assert.match(shell,/root\.buzzPendingSendFid/);
  assert.match(profile,/visible: true\s*\n\s*Layout\.fillWidth: true/,
    'own mobile profile must expose actions');
  assert.match(shell,/openProofConnections\.sendGenyBuzz\(/);
  assert.match(shell,/objectName: "genyBuzzIncomingToast"/);
  assert.match(shell,/openProofConnections\.acknowledgeGenyBuzz/);
  assert.match(shell,/interval: 30_000/);
  assert.doesNotMatch(read('services/account-sync/genybuzz.mjs'),
    /castSubmit|writeGateway|submitCast|walletSign/);
});

test('GenyBuzz strings are present in all compiled translations',()=>{
  const ids=['enable','disable','send','sent','slow_down','not_available',
    'error','received','from_fid','view','service_unavailable'];
  const proErrors=['quote_wallet_required','quote_retry_notice',
    'checkout_verify_wallet','checkout_verifying_wallet',
    'wallet_verification_unavailable','quote_rate_unavailable',
    'quote_chain_unavailable'];
  for(const locale of ['en_US','fa_IR','de_DE','ja_JP','ko_KR','zh_CN',
    'hi_IN','ar_SA','es_ES','fr_FR','pt_BR','ru_RU']){
    const file=read('ui/i18n/qml_'+locale+'.ts');
    for(const id of ids)
      assert.ok(file.includes('id="genycaster.buzz.'+id+'"'),
        locale+' misses '+id);
    for(const id of proErrors)
      assert.ok(file.includes('id="genycaster.pro.'+id+'"'),
        locale+' misses '+id);
  }
});
