import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source=p=>readFileSync(p,'utf8');
const shell=source('ui/components/shell/AppShell.qml');
const setup=source('ui/components/setup/InitialSetup.qml');
const bridge=source('src/presentation/qt/presentation_bridge.cpp');
const iconTest=source('tests/unit/presentation_icon_tests.cpp');

test('signer authorization uses App Center Neon material and a real busy spinner',()=>{
  const dialog=shell.slice(shell.indexOf('id: interactionAuthorizationDialog'),
    shell.indexOf('GenyProDetailsDrawer {',shell.indexOf('id: interactionAuthorizationDialog')));
  assert.match(dialog,/objectName: "signerRecoverySecurityNotice"/);
  assert.match(dialog,/fillColor: Theme\.warningSurface/);
  assert.match(dialog,/text: Theme\.iconWarning/);
  assert.match(dialog,/objectName: "signerAuthorizationNeonProgress"/);
  assert.match(dialog,/neonBusy: presentationBridge\.signerBusy/);
  assert.match(dialog,/ambientNeon: presentationBridge\.signerBusy/);
  assert.match(dialog,/GenySpinner \{\s*visible: presentationBridge\.signerBusy\s*running: visible/);
  assert.match(dialog,/Theme\.successSurface/);
  const action=dialog.slice(dialog.indexOf('objectName: "authorizeSelectedProtocolSigner"'));
  assert.match(action,/busy: interactionAuthorizationDialog\.authorizationSubmitted\s*&& presentationBridge\.signerBusy/);
  assert.match(action,/genycaster\.setup\.signer_verification_title/);
  assert.match(action,/genycaster\.setup\.signer_verified_title/);
  assert.match(action,/enabled: !presentationBridge\.signerBusy/);
  assert.match(action,/if \(presentationBridge\.writeReady\) \{\s*interactionAuthorizationDialog\.close\(\)/);
  assert.doesNotMatch(dialog,/progress\s*:\s*0\.\d+/, 'Never invent network verification percentages');
});

test('both Farcaster and HyperSnap setup flows get a distinct semantic warning and a separate signer explanation',()=>{
  const begin=setup.indexOf('id: signInStage');
  const end=setup.indexOf('id: verifyingStage',begin);
  const stage=setup.slice(begin,end);
  assert.match(stage,/objectName: "setupRecoverySecurityNotice"/);
  assert.match(stage,/text: Theme\.iconWarning/);
  assert.match(stage,/fillColor: Theme\.warningSurface/);
  assert.match(stage,/root\.recoveryProtocol === "hypersnap-v2"\s*\? qsTrId\("genycaster\.setup\.native_recovery_privacy"\)\s*: qsTrId\("genycaster\.setup\.recovery_privacy"\)/);
  assert.match(stage,/objectName: "nativeRecoveryAutomaticSignerNotice"/);
  assert.match(stage,/fillColor: Theme\.primarySurface/);
  assert.match(stage,/text: Theme\.iconCheck/);
  assert.match(stage,/genycaster\.setup\.recovery_automatic_signer_notice/);
});

test('three new signer and security messages are translated, non-empty and catalogued in all 12 locales',()=>{
  const keys=['recovery_notice_title','signer_verification_title','signer_verified_title'];
  const catalog=source('ui/i18n/catalog.cpp');
  for(const key of keys)
    assert.match(catalog,new RegExp('qtTrId\\("genycaster\\.setup\\.'+key+'"\\)'));
  for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
      'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
    const xml=source('ui/i18n/qml_'+locale+'.ts');
    for(const key of keys){
      const re=new RegExp('<message id="genycaster\\.setup\\.'+key+'">([\\s\\S]*?)<\\/message>');
      const matches=[...xml.matchAll(new RegExp(re.source,'g'))];
      assert.equal(matches.length,1,locale+' '+key+' duplicate or missing');
      const value=matches[0][1].match(/<translation>([\s\S]*?)<\/translation>/)?.[1]||'';
      assert.ok([...value.trim()].length>=4,locale+' '+key+' untranslated');
      assert.notEqual(value,'genycaster.setup.'+key);
    }
  }
});

test('late profile result cannot reopen Profile after Back to Cast',()=>{
  const method=bridge.slice(bridge.indexOf('void PresentationBridge::applyDetail('),
    bridge.indexOf('void PresentationBridge::',bridge.indexOf('void PresentationBridge::applyDetail(')+8));
  const guard=method.indexOf('&& m_activeView != view');
  const assign=method.indexOf('m_activeView = std::move(view)');
  assert.ok(guard>=0 && assign>guard,'stale view is rejected before mutating active view');
  assert.match(method,/view == QStringLiteral\("profile"\) \|\| view == QStringLiteral\("cast"\)/);
  assert.match(method,/receivedCastId\.compare\(m_currentCastId, Qt::CaseInsensitive\) != 0/);
  assert.match(iconTest,/bridge\.returnFromDetail\(\);\s*assert\(bridge\.activeView\(\) == QStringLiteral\("cast"\)\)/);
  assert.match(iconTest,/bridge\.applyDetail\(QStringLiteral\("profile"\)[\s\S]*?Delayed profile[\s\S]*?assert\(bridge\.activeView\(\) == QStringLiteral\("cast"\)\)/);
  const list=bridge.slice(bridge.indexOf('void PresentationBridge::applyList('),
    bridge.indexOf('void PresentationBridge::',bridge.indexOf('void PresentationBridge::applyList(')+12));
  assert.match(list,/view == QStringLiteral\("profile"\)/);
  assert.match(list,/m_activeView != QStringLiteral\("profile"\)/);
  assert.match(list,/selector != m_profileFeed/);
  assert.match(list,/view == QStringLiteral\("feed"\)/);
  assert.match(iconTest,/bridge\.applyList\(QStringLiteral\("profile"\)[\s\S]*?assert\(bridge\.activeView\(\) == QStringLiteral\("cast"\)\)/);
});

test('Back skips self-referential profile frames, then restores a real prior destination',()=>{
  const method=bridge.slice(bridge.indexOf('void PresentationBridge::returnFromDetail()'),
    bridge.indexOf('void PresentationBridge::loadMore()',bridge.indexOf('void PresentationBridge::returnFromDetail()')));
  assert.match(method,/const bool identicalProfileFid/);
  assert.match(method,/previous\.profileFid == m_profileFid/);
  assert.match(method,/previous\.ownProfile == ownProfileNavigation\(\)/);
  assert.match(method,/const bool identicalProfileUsername/);
  assert.match(method,/if \(identicalCast \|\| identicalProfileFid \|\| identicalProfileUsername\)\s*continue/);
  assert.match(method,/openCast\(previous\.castId\)/);
  assert.match(method,/showFeed\(\)/);
});
