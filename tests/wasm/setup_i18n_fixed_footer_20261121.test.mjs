import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = name => readFileSync(new URL('../../'+name,import.meta.url),'utf8');
const setup = read('ui/components/setup/InitialSetup.qml');
const shell = read('ui/components/shell/AppShell.qml');
const locales = ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
                 'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
const recentIds = [
  "genycaster.setup.hypersnap_sync_pause",
  "genycaster.setup.hypersnap_checking",
  "genycaster.setup.network_status",
  "genycaster.setup.registration_sync_paused",
  "genycaster.setup.network_syncing",
  "genycaster.setup.farcaster_paid_registration",
  "genycaster.setup.farcaster_create_external",
  "genycaster.setup.farcaster_external_help",
  "genycaster.setup.sign_in_existing",
  "genycaster.setup.sponsored_code_available",
  "genycaster.setup.sponsor_checking",
  "genycaster.setup.sponsor_unavailable",
  "genycaster.setup.hypersnap_chain",
  "genycaster.setup.registration_unavailable",
  "genycaster.setup.network_checking",
  "genycaster.setup.hypersnap_nodes_syncing",
  "genycaster.setup.hypersnap_checking_short",
  "genycaster.setup.check_network",
  "genycaster.setup.cancel_registration",
  "genycaster.setup.continue_sponsored",
  "genycaster.setup.back_to_registration",
  "genycaster.setup.recovery_words_verified_status",
  "genycaster.setup.keys_on_device",
  "genycaster.setup.sponsor_code_explanation",
  "genycaster.setup.sponsor_code_paste",
  "genycaster.setup.sponsor_code_label",
  "genycaster.setup.continue_username",
  "genycaster.setup.create_farcaster",
  "genycaster.setup.check_account_status",
  "genycaster.setup.create_account",
  "genycaster.setup.create_native_identity",
  "genycaster.setup.await_network_confirmation",
  "genycaster.setup.hypersnap_unavailable",
  "genycaster.setup.confirmation_pending_dont_retry",
  "genycaster.setup.account_creation_paused",
  "genycaster.setup.backup",
  "genycaster.setup.checking_request",
  "genycaster.setup.request",
  "genycaster.setup.confirmed",
  "genycaster.setup.keep_recovery_words",
  "genycaster.setup.check_status",
  "genycaster.setup.leave_for_now",
  "genycaster.setup.leave_keeps_request",
  "genycaster.setup.no_request_submitted",
  "genycaster.setup.account_found_signing_in",
  "genycaster.setup.hide_diagnostics",
  "genycaster.setup.developer_details",
  "genycaster.setup.prepare_new_attempt",
  "genycaster.setup.reset_expired_attempt",
  "genycaster.setup.signed_in_signer_separate",
  "genycaster.setup.check_signer_status",
  "genycaster.setup.signer_read_only_check"
];

test('onboarding has one fixed bottom footer, with scrolling content above it',()=>{
  assert.match(setup,/readonly property bool showsPrimaryCta: \[0,1,2,6,7\]\.includes\(currentStep\)/);
  assert.match(setup,/objectName: "setupActionFooter"/);
  assert.match(setup,/anchors.bottom: setupActionFooter.top/);
  assert.match(setup,/anchors.bottom: parent.bottom/);
  assert.match(setup,/x: setupCanvas.formStart/);
  assert.match(setup,/width: setupCanvas.formWidth/);
  assert.match(setup,/id: stageAction/);
  assert.match(setup,/anchors.verticalCenter: parent.verticalCenter/);
  assert.match(setup,/enabled: !root.stageTransitioning && root.actionEnabled\(\)/);
  assert.match(setup,/onClicked: root.nextStep\(\)/);
  assert.match(setup,/objectName: "setupWelcomeNativeApps"/);
  assert.match(setup,/readonly property bool stackWelcomeActions: phone && welcomeNativeAppsVisible/);
  assert.match(setup,/y: root\.stackWelcomeActions\s*\? stageAction\.y \+ stageAction\.height \+ Theme\.space2\s*: stageAction\.y/);
  assert.match(setup,/height: \(root\.stackWelcomeActions\s*\? stageAction\.height \+ Theme\.space2 \+ setupWelcomeNativeApps\.height/);
  assert.doesNotMatch(setup,/anchors.top: stageLoader.bottom/);
  assert.match(setup,/implicitHeight: stageLoader.implicitHeight\s+height: implicitHeight/);
});

test('all 52 recently introduced setup and signer messages have 12 real translations',()=>{
  for(const id of recentIds) {
    assert.ok(setup.includes('qsTrId("'+id+'")') ||
              shell.includes('qsTrId("'+id+'")'),id);
  }
  for(const locale of locales) {
    const ts=read('ui/i18n/qml_'+locale+'.ts');
    for(const id of recentIds) {
      const escaped=id.replace(/[.*+?^$\{\}()|[\]\\]/g,'\\$&');
      const pattern=new RegExp('<message id="'+escaped+'">\\s*<source>([\\s\\S]*?)<\\/source>\\s*<translation>([\\s\\S]*?)<\\/translation>');
      const match=ts.match(pattern);
      assert.ok(match,locale+' missing '+id);
      assert.ok(match[2].trim(),locale+' empty '+id);
      if(locale!=='en_US')assert.notEqual(match[1],match[2],locale+' untranslated '+id);
    }
  }
  assert.equal(setup.includes("qsTr("),false);
  assert.equal(shell.includes("qsTr("),false);
});
