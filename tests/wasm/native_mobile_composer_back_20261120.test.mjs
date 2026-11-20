import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read = path => readFileSync(new URL('../../'+path, import.meta.url), 'utf8');

test('Farcaster Safari Cast intent keeps its draft through signer verification', () => {
  const shell = read('ui/components/shell/AppShell.qml');
  const open = shell.slice(shell.indexOf('function openComposer('), shell.indexOf('function openMiniAppComposer('));
  const resolve = shell.slice(shell.indexOf('function resolvePendingComposer()'), shell.indexOf('Connections {', shell.indexOf('function resolvePendingComposer()')));
  assert.ok(open.includes('root.pendingComposerIntent = {'));
  assert.ok(open.includes('root.requestInteractionAuthorization()'));
  assert.ok(!open.includes('presentationBridge.signerPhase === "checking"'));
  assert.ok(resolve.includes('if (!presentationBridge.writeReady)'));
  assert.ok(resolve.includes('root.requestInteractionAuthorization()'));
  assert.ok(resolve.includes('if (presentationBridge.isAuthenticated && presentationBridge.writeReady)'));
  assert.ok(shell.includes('if (!authorizationSubmitted)'));
  assert.ok(shell.includes('root.pendingComposerIntent = null'));
});

test('Android hardware back is routed to the topmost dialog before social history', () => {
  const shell = read('ui/components/shell/AppShell.qml');
  const main = read('ui/app/main.cpp');
  const composer = read('ui/components/composer/ComposerDrawer.qml');
  const back = shell.slice(shell.indexOf('function handlePlatformBack()'), shell.indexOf('GenyModal {', shell.indexOf('function handlePlatformBack()')));
  for (const item of ['interactionAuthorizationDialog.opened', 'localeSettingsOpen',
    'composerDrawer.dismissTopLayer()', 'initialSetup.previousStep()',
    'returnFromCastDetail()', 'root.shellRoute = ""']) assert.ok(back.includes(item), item);
  assert.ok(back.indexOf('composerDrawer.dismissTopLayer()') < back.indexOf('returnFromCastDetail()'));
  for (const item of ['class AndroidBackDispatcher', 'Qt::Key_Back', 'QEvent::KeyPress',
    'inputMethod()->isVisible()', 'handlePlatformBack', 'installEventFilter']) assert.ok(main.includes(item), item);
  assert.ok(composer.includes('function dismissTopLayer()'));
});

test('Native handset layout stays mobile regardless high-density display width', () => {
  const shell = read('ui/components/shell/AppShell.qml');
  const dialogs = read('ui/components/controls/GenyAdaptiveDialog.qml');
  const settings = read('ui/components/settings/SettingsPopup.qml');
  const setup = read('ui/components/setup/InitialSetup.qml');
  assert.ok(shell.includes('readonly property bool phonePlatform:'));
  assert.ok(shell.includes('readonly property bool mobile: phonePlatform ||'));
  assert.ok(shell.includes('readonly property bool compact: phonePlatform ||'));
  for (const source of [dialogs,settings,setup]) assert.ok(source.includes('Qt.platform.os === "ios"'));
  assert.ok(settings.includes('id: textSelectionDescription'));
  assert.ok(settings.includes('Layout.maximumWidth: Theme.scaleUi(360)'));
  assert.ok(settings.includes('textSelectionDescription.implicitHeight'));
});

test('Composer picker position has no y self-dependency or y transition', () => {
  const composer = read('ui/components/composer/ComposerDrawer.qml');
  const pickers = composer.slice(composer.indexOf('id: channelPicker'), composer.indexOf('id: mediaFileDialog'));
  assert.ok(!pickers.includes('triggerOrigin.y - y'));
  assert.ok(!pickers.includes('property: "y"'));
  assert.ok(pickers.includes('channelPicker.close()'));
});

test('Derived gateway POSTs avoid broken HTTP2 stream and WASM Lottie carries Timeline', () => {
  for (const path of ['ui/app/social/cast_views.cpp','ui/app/browser/reactions.cpp']) {
    assert.ok(read(path).includes('request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false)'), path);
  }
  const cmake = read('ui/CMakeLists.txt');
  assert.ok(cmake.includes('find_package(Qt6 REQUIRED COMPONENTS QuickTimeline)'));
  assert.ok(cmake.includes('Qt6::qtquicktimelineplugin'));
});
