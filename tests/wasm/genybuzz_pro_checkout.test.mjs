import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
const pro=read('ui/components/profile/GenyProDetailsDrawer.qml');
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
  assert.doesNotMatch(proceed.slice(0,proceed.indexOf('if \(root\.txSubmitted\)')),
    /if \(root\.walletAvailable[^\n]*proCheckout\.pay\(/);
  assert.match(pro,/visible: !root\.checkoutActive && openProofConnections\s*\n\s*&& String\(openProofConnections\.proBillingError/);
  assert.match(pro,/root\.purchaseIntent = false\s*\n\s*root\.lastRequestedAsset = ""/);
});

test('GenyBuzz remains app-owned, opt-in and DM-context-ready without a protocol writer',()=>{
  assert.match(cloud,/handleGenyBuzz\(/);
  assert.match(migration,/CREATE TABLE IF NOT EXISTS genybuzz_preferences/);
  assert.match(migration,/enabled boolean NOT NULL DEFAULT false/);
  assert.match(migration,/CREATE TABLE IF NOT EXISTS genybuzz_events/);
  assert.match(migration,/context IN \('profile','dm'\)/);
  assert.match(store,/pg_advisory_xact_lock/);
  assert.match(store,/recipient_count/);
  assert.match(store,/muted_profiles/);
  assert.match(header,/Q_INVOKABLE void sendGenyBuzz\(qulonglong targetFid/);
  assert.match(controller,/authorizeCloudRequest\(request\)/);
  assert.match(controller,/cancelNetworkReply\(m_buzzInboxRead, this\)/);
  assert.match(profile,/objectName: "genyBuzzReceiveToggle"/);
  assert.match(profile,/objectName: "genyBuzzSendFromProfile"/);
  assert.match(shell,/openProofConnections\.sendGenyBuzz\(/);
  assert.match(shell,/objectName: "genyBuzzIncomingToast"/);
  assert.match(shell,/openProofConnections\.acknowledgeGenyBuzz/);
  assert.match(shell,/interval: 30_000/);
  assert.doesNotMatch(read('services/account-sync/genybuzz.mjs'),
    /castSubmit|writeGateway|submitCast|walletSign/);
});

test('GenyBuzz strings are present in all compiled translations',()=>{
  const ids=['enable','disable','send','sent','slow_down','not_available',
    'error','received','from_fid','view'];
  for(const locale of ['en_US','fa_IR','de_DE','ja_JP','ko_KR','zh_CN',
    'hi_IN','ar_SA','es_ES','fr_FR','pt_BR','ru_RU']){
    const file=read('ui/i18n/qml_'+locale+'.ts');
    for(const id of ids)
      assert.ok(file.includes('id="genycaster.buzz.'+id+'"'),
        locale+' misses '+id);
  }
});
