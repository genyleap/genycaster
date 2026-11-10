import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = name => readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('sidebar account card presents four factual metrics including tappable full FID',()=>{
 const qml=read('ui/components/profile/ProfileTrustCard.qml');
 const face=qml.slice(qml.indexOf('id: frontFace'),qml.indexOf('id: backFace'));
 assert.match(face,/objectName: "sidebarFidMetric"/);
 assert.match(face,/valueText: Number\(root\.fid\) > 0\s*\? LocaleFormat\.formatInteger\(root\.fid\)/);
 assert.match(face,/labelText: "FID"/);
 assert.match(face,/interactive: true\s*onActivated: root\.requestProfile\(\)/);
 assert.match(face,/visible: !root\.compact && root\.bio\.length > 0/);
 assert.match(face,/text: root\.compact && root\.proEligible\s*\? qsTrId\("genycaster\.pro\.short"\)/);
 assert.match(face,/Layout\.minimumWidth: root\.compact \? 84 : 0/);
 assert.match(face,/wrapMode: Text\.WrapAnywhere/);
 assert.match(face,/maximumLineCount: 2\s*elide: Text\.ElideNone/);
 assert.match(qml,/text: parent\.labelText[\s\S]*?wrapMode: Text\.Wrap[\s\S]*?elide: Text\.ElideNone/);
 assert.match(qml,/objectName: "sidebarStreakRing"/);
 assert.match(qml,/onTapped: root\.toggleFace\(\)/);
 assert.match(qml,/qsTrId\("genycaster\.streak\.next"\)/);
});

test('drawer describes backend-verified Pro capabilities without using cast thread dialog strings',()=>{
 const drawer=read('ui/components/profile/GenyProDetailsDrawer.qml');
 const policy=read('services/account-sync/pro_billing.mjs');
 const composer=read('ui/components/composer/ComposerDrawer.qml');
 const settings=read('ui/components/settings/SettingsPopup.qml');
 const chooser=read('ui/components/profile/CollectionChooser.qml');
 assert.match(policy,/concurrent_miniapps:1,market_pulse:4/);
 assert.match(policy,/concurrent_miniapps:4,market_pulse:16/);
 assert.match(policy,/cast_characters:4096,image_embeds:4/);
 assert.match(policy,/monthlyUsdCents:Number\(cents\)/);
 assert.match(policy,/expires_at:eligible\?until:null/);
 assert.match(composer,/maxComposerCharacters: verifiedGenyPro/);
 assert.match(settings,/marketAssetLimit: marketProEligible \? 16 : 4/);
 assert.match(chooser,/enabled: root\.proEligible/);
 for(const id of [
  'feature_miniapps_title','feature_miniapps_desc',
  'feature_collections_desc','feature_connections_desc','feature_market_desc',
  'feature_composer_title','feature_composer_desc'
 ])assert.match(drawer,new RegExp('genycaster\\.pro\\.'+id));
 assert.doesNotMatch(drawer,/genycaster\.pro\.thread_explanation/);
 assert.doesNotMatch(drawer,/genycaster\.miniapp\.session_limit/);
 assert.match(drawer,/genycaster\.pro\.status_verified/);
 assert.match(drawer,/genycaster\.pro\.monthly_price/);
 assert.match(drawer,/genycaster\.pro\.verify_payment/);
 assert.match(drawer,/openProofConnections\.requestProPaymentQuote\(root\.selectedAsset\)/);
});

test('Pro checkout uses verified server quote and never grants itself locally',()=>{
 const drawer=read('ui/components/profile/GenyProDetailsDrawer.qml');
 const button=read('ui/components/controls/ActionButton.qml');
 const header=read('ui/app/identity/connections.hpp');
 const cpp=read('ui/app/identity/connections.cpp');
 assert.match(header,/Q_PROPERTY\(bool proBillingBusy READ proBillingBusy NOTIFY proBillingChanged\)/);
 assert.match(header,/proBillingBusy\(\) const noexcept/);
 assert.match(header,/m_proBillingRequest != nullptr/);
 assert.match(drawer,/readonly property bool busy: \(openProofConnections && openProofConnections\.proBillingBusy\)/);
 assert.match(drawer,/requestProPaymentQuote\(root\.selectedAsset\)/);
 assert.match(drawer,/verifyProPayment\(root\.paymentHash\.trim\(\)\)/);
 assert.match(drawer,/proCheckout\.pay\(\)/);
 assert.match(cpp,/authorizeCloudRequest\(request\)/);
 assert.match(cpp,/\/api\/v1\/cloud\/pro\/verify/);
 assert.match(cpp,/fetchProEntitlements\(true\)/);
 assert.match(button,/running: control\.busy && control\.spinIconWhileBusy/);
});
test('Pro descriptions are translated and contain no unfilled %1 in feature details',()=>{
 const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN',
  'ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 const keys=['status_verified','monthly_price','manual_renewal',
  'verify_payment','checkout_continue','checkout_open_wallet','checkout_usdc_hint',
  'payment_instructions',
  'feature_miniapps_title','feature_miniapps_desc','feature_collections_desc',
  'feature_connections_desc','feature_market_desc','feature_composer_title',
  'feature_composer_desc'];
 for(const lang of locales){
  const xml=read('ui/i18n/qml_'+lang+'.ts');
  for(const key of keys){
   const needle='id="genycaster.pro.'+key+'"';
   assert.equal(xml.split(needle).length-1,1,lang+':'+key);
  }
  assert.match(xml,/<source>Pro supports up to 4 Mini Apps at once \(Basic: 1\)\./);
  assert.match(xml,/<source>Track up to 16 Market Pulse tokens \(Basic: 4\)\./);
  const featureSection=xml.slice(xml.indexOf('id="genycaster.pro.feature_composer_desc"'),
   xml.indexOf('id="genycaster.pro.thread_title"'));
  assert.doesNotMatch(featureSection,/%1/);
 }
});

test('Pro drawer loads server-owned public price and recovers account authorization',()=>{
 const drawer=read('ui/components/profile/GenyProDetailsDrawer.qml');
 const shell=read('ui/components/shell/AppShell.qml');
 const cpp=read('ui/app/identity/connections.cpp');
 const header=read('ui/app/identity/connections.hpp');
 const cloud=read('services/account-sync/cloud_state.mjs');
 assert.match(drawer,/monthlyUsdCents: Number\(plan\.price_usd_cents \|\| 0\)/);
 assert.doesNotMatch(drawer,/monthlyUsdCents: Number\(membership\.monthly_usd_cents/);
 assert.match(drawer,/openProofConnections\.fetchProPlan\(true\)/);
 assert.match(drawer,/openProofConnections\.retryProEntitlements\(\)/);
 assert.match(drawer,/root\.canRequestQuote/);
 assert.match(drawer,/root\.accountRequested\(\)/);
 assert.match(shell,/onAccountRequested: root\.openAccountPopup\(\)/);
 assert.match(header,/Q_PROPERTY\(QVariantMap proPlan READ proPlan NOTIFY proPlanChanged\)/);
 assert.match(header,/Q_PROPERTY\(qulonglong activeAccountFid READ activeAccountFid NOTIFY stateChanged\)/);
 assert.match(cpp,/\/api\/v1\/cloud\/pro\/plan/);
 assert.match(cpp,/void OpenProofConnectionsController::fetchProPlan/);
 assert.match(cloud,/entity==='pro\/plan'/);
 assert.match(cloud,/price_usd_cents:billingPolicy\.monthlyUsdCents/);
 assert.match(cloud,/if \(!owner\) \{respond\(response,401/);
});

test('signed-in Pro checkout verifies its existing FID without reopening login',()=>{
 const drawer=read('ui/components/profile/GenyProDetailsDrawer.qml');
 const header=read('ui/app/identity/connections.hpp');
 const implementation=read('ui/app/identity/connections.cpp');
 assert.match(header,/Q_PROPERTY\(bool cloudSessionBusy READ cloudSessionBusy NOTIFY stateChanged\)/);
 assert.match(drawer,/readonly property bool appSignedIn: presentationBridge/);
 assert.match(drawer,/Number\(presentationBridge\.accountFid\)/);
 assert.match(drawer,/Number\(openProofConnections\.activeAccountFid \|\| 0\)/);
 assert.match(drawer,/root\.appSignedIn && !root\.cloudReady/);
 assert.match(drawer,/root\.startedVerification = true\s+openProofConnections\.authorizeAccount\(\)/);
 assert.match(drawer,/Qt\.openUrlExternally\(openProofConnections\.approvalUrl\)/);
 assert.match(drawer,/openProofConnections\.stopAuthorization\(\)/);
 assert.match(drawer,/if \(!root\.appSignedIn\) \{ root\.accountRequested\(\); return \}/);
 assert.match(drawer,/root\.restoringSession/);
 assert.match(implementation,/m_sessionRequest=reply;\s+emit stateChanged\(\);/);
 assert.doesNotMatch(drawer,/openProofConnections\.retryProEntitlements\(\)\s+root\.accountRequested\(\)/);
 for(const lang of ['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR',
   'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']) {
  const ts=read('ui/i18n/qml_'+lang+'.ts');
  for(const id of ['restoring_payment_session',
      'approve_payment_identity','verify_payment_identity','verify_with_farcaster'])
   assert.equal(ts.split('id="genycaster.pro.'+id+'"').length-1,1,lang+':'+id);
 }
});

test('WASM Pro uses an isolated browser EIP-1193 wallet without localhost listener',()=>{
 const bridge=read('ui/app/identity/browser_pro_wallet.hpp');
 const app=read('ui/app/main.cpp');
 const checkout=read('ui/app/identity/pro_checkout.cpp');
 assert.match(app,/genycaster::ui::identity::BrowserProWallet proBrowserWallet\{proCheckout\}/);
 assert.match(bridge,/window\.ethereum\.request/);
 for(const method of ['eth_requestAccounts','eth_chainId','wallet_switchEthereumChain','eth_sendTransaction'])
   assert.match(bridge,new RegExp(method));
 assert.match(bridge,/state\.pending\.add\(id\)/);
 assert.match(bridge,/completion\(object\.value/);
 assert.doesNotMatch(bridge,/127\.0\.0\.1|localhost|privateKey|mnemonic|seedPhrase/);
 assert.match(checkout,/emit transactionSubmitted\(m_quoteId,hash\)/);
 assert.match(app,/ProCheckoutController::transactionSubmitted,[\s\S]*?OpenProofConnectionsController::verifyProPaymentForQuote/);
});

test('own mobile profile shows accurate Pro, Basic or unknown state beside name',()=>{
 const qml=read('ui/components/profile/ProfileSurface.qml');
 const badge=read('ui/components/profile/ProfileProBadge.qml');
 assert.match(qml,/objectName: "profileIdentityRow"[\s\S]*?objectName: "compactOwnAccountProStatus"/);
 assert.match(qml,/objectName: "compactOwnAccountProStatus"[\s\S]*?visible: root\.compact && root\.isOwnProfile[\s\S]*?showUpgrade: true[\s\S]*?entitlement: root\.ownProEntitlement/);
 assert.match(qml,/openProofConnections\.cloudAuthorizationReady[\s\S]*?Number\(openProofConnections\.activeAccountFid\) === Number\(root\.activeDetail\.fid \|\| 0\)/);
 assert.match(qml,/objectName: "profileAchievementBadges"[\s\S]*?showUpgrade: true\s+visible: root\.isOwnProfile && !root\.compact/);
 assert.match(badge,/entitlement\.status === "verified"\s*&& entitlement\.tier === "pro" && entitlement\.eligible === true/);
 assert.match(badge,/entitlement\.status === "verified"\s*&& entitlement\.tier === "basic" && entitlement\.eligible === false/);
 assert.match(badge,/get_pro/);
 assert.match(badge,/check_pro_status/);
 assert.match(badge,/onClicked: \{[\s\S]*?root\.detailsRequested\(\)/);
 const locales=['ar_SA','de_DE','en_US','es_ES','fa_IR','fr_FR','hi_IN',
  'ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
 for(const lang of locales){
  const xml=read('ui/i18n/qml_'+lang+'.ts');
  for(const id of ['get_pro','check_pro_status','basic_status_hint','status_unknown_hint'])
   assert.equal(xml.split('id="genycaster.pro.'+id+'"').length-1,1,lang+':'+id);
 }
});
