import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');

test('verified SIWF remains bound to active Farcaster FID and linked custody/auth wallet',()=>{
  const runtime=read('ui/app/miniapp/runtime.cpp');
  const auth=read('src/providers/compatibility/farcaster_auth/auth.cpp');
  assert.match(runtime,/m_controller\.user\(\)\.value\(QStringLiteral\("fid"\)\)/);
  assert.match(runtime,/verifyMiniAppSignIn\(/);
  assert.match(runtime,/\{QStringLiteral\("message"\), message\}/);
  assert.match(runtime,/\{QStringLiteral\("signature"\), signature\}/);
  assert.match(auth,/if \(acceptAuthAddress\)/);
  assert.match(auth,/isActiveAuthAddress\(expectedAddress, fid\)/);
  assert.match(auth,/fidForCustody\(expectedAddress\)/);
  assert.match(runtime,/reportMiniAppSignInVerificationFailed\(\)/);
  assert.doesNotMatch(runtime,/fakeSignIn|mockSignature|bypassSiwf/);
});

test('SIWF verification failure uses first-party explanatory dialog, not a forged SDK response',()=>{
  const host=read('ui/app/miniapp/host.hpp');
  const impl=read('ui/app/miniapp/host.cpp');
  const shell=read('ui/components/shell/AppShell.qml');
  assert.match(host,/Q_PROPERTY\(bool miniAppSignInVerificationFailed/);
  assert.match(impl,/void MiniAppHostController::reportMiniAppSignInVerificationFailed\(\)/);
  assert.match(shell,/id: miniAppSignInFailureDialog/);
  assert.match(shell,/genycaster\.miniapp\.signin_wallet_description/);
  assert.match(shell,/miniAppHost\.dismissMiniAppSignInVerificationFailed\(\)/);
  assert.match(shell,/miniAppSignInFailureDialog\.open\(\)/);
  assert.match(shell,/miniAppSignInFailureDialog\.close\(\)/);
});

test('all 12 locales contain sign-in failure instructions with no native browser controls',()=>{
  const locales=['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR',
    'hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN'];
  for(const locale of locales){
    const catalog=read('ui/i18n/qml_'+locale+'.ts');
    assert.match(catalog,/<message id="genycaster\.miniapp\.signin_wallet_title">/);
    assert.match(catalog,/<message id="genycaster\.miniapp\.signin_wallet_description">/);
  }
  const shell=read('ui/components/shell/AppShell.qml');
  assert.match(shell,/GenyAdaptiveDialog \{\s*id: miniAppSignInFailureDialog/);
  assert.doesNotMatch(shell,/TextField \{\s*id: miniAppSignInFailureDialog/);
});

test('explorer error is scoped to failing app and cleared on next successful launch',()=>{
  const apps=read('ui/components/apps/AppsSurface.qml');
  assert.match(apps,/const failingName = String\(root\.pendingLaunchApp/);
  assert.match(apps,/failingName \+ " — " \+ miniAppHost\.errorText/);
  assert.match(apps,/else if \(miniAppHost\.active\) \{/);
  assert.match(apps,/launchErrorTimeout\.stop\(\)/);
});


test('native Mini App drawer is hidden before SIWF error modal and restored only after dismissal',()=>{
  const shell=read('ui/components/shell/AppShell.qml');
  const task=read('ui/app/miniapp/tasks.hpp');
  const runtime=read('ui/app/miniapp/runtime.cpp');
  const start=shell.indexOf('id: miniAppSignInFailureDialog');
  const end=shell.indexOf('id: walletBrowserPicker',start);
  assert.ok(start>0 && end>start);
  const dialog=shell.slice(start,end);
  assert.match(dialog,/popupZ: 22000/);
  assert.match(dialog,/property string suspendedTaskId: ""/);
  assert.match(dialog,/onMiniAppSignInVerificationFailedChanged\(\)/);
  assert.match(dialog,/miniAppTaskManager\.minimizeTask\(String\(current\.taskId\)\)/);
  assert.match(dialog,/miniAppSignInFailureDialog\.open\(\)/);
  assert.ok(dialog.indexOf('miniAppTaskManager.minimizeTask(')
    < dialog.indexOf('miniAppSignInFailureDialog.open()'));
  assert.match(dialog,/onDidClose: \{/);
  assert.match(dialog,/miniAppTaskManager\.focusTask\(taskId\)/);
  assert.match(task,/Q_INVOKABLE void minimizeTask\(const QString& taskId\)/);
  assert.match(task,/Q_INVOKABLE void focusTask\(const QString& taskId\)/);
  assert.match(runtime,/target->browser->setVisible\(false\)/);
  assert.doesNotMatch(dialog,/miniAppHost\.close\(\)/);
});

test('detail sheet avoids null geometry binding during teardown',()=>{
  const apps=read('ui/components/apps/AppsSurface.qml');
  assert.match(apps,/width: detailScroll \? detailScroll\.width : 0/);
  assert.match(apps,/\(detailScroll \? detailScroll\.width : 0\)/);
  assert.doesNotMatch(apps,/^\s*width: detailScroll\.width\s*$/m);
});
