import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';

const code=name=>readFileSync(new URL('../../'+name,import.meta.url),'utf8');

test('own profile refresh reads both canonical profile fields and account draft',()=>{
  const profile=code('ui/components/ProfileSurface.qml');
  assert.match(profile,/function refreshProfileAndAccount\(\) \{[\s\S]*?root\.refreshRequested\(\)[\s\S]*?profileRefreshRetry\.restart\(\)[\s\S]*?openProofConnections\.refreshAccountDraft\(\)/);
  assert.match(profile,/onClicked: root\.refreshProfileAndAccount\(\)/);
  assert.match(profile,/onRefreshRequested: root\.refreshProfileAndAccount\(\)/);
  assert.match(profile,/id: profileRefreshRetry[\s\S]*?if \(!openProofConnections\.cloudBusy\) \{[\s\S]*?fetchCloudProfileExtras\(\)/);
  assert.match(profile,/property int profileExtrasGeneration: 0/);
  assert.match(profile,/const generation = root\.profileExtrasGeneration/);
  assert.match(profile,/root\.profileExtrasGeneration\+\+/);
});
test('fresh draft read is performed when the ordinary composer opens',()=>{
  const composer=code('ui/components/ComposerDrawer.qml');
  const header=code('ui/mobile/openproof_connections_controller.hpp');
  const cpp=code('ui/mobile/openproof_connections_controller.cpp');
  assert.match(header,/Q_INVOKABLE void refreshAccountDraft\(\)/);
  assert.match(cpp,/void OpenProofConnectionsController::refreshAccountDraft\(\)[\s\S]*?pumpDraftChanges\(\)/);
  assert.match(composer,/onOpened: \{[\s\S]*?if \(!root\.transientDraft && !root\.parentCast && !root\.quotedCast\)[\s\S]*?refreshAccountDraft\(\)/);
  assert.match(composer,/function onAccountDraftApplied\(\) \{[\s\S]*?mobilePlainEditor\.text = richComposer\.safeText/);
});
test('incoming draft updates an untouched open composer but never overwrites any editing session',()=>{
  const header=code('src/presentation/qt/rich_composer_controller.hpp');
  const cpp=code('src/presentation/qt/rich_composer_controller.cpp');
  assert.match(header,/void accountDraftApplied\(\)/);
  assert.match(cpp,/if \(composerActive && !applyingRemote\) composerTouched = true;/);
  assert.match(cpp,/if \(active\) m_impl->composerTouched=false;/);
  assert.match(cpp,/m_impl->composerActive && m_impl->composerTouched/);
  assert.match(cpp,/m_impl->dirty \|\| m_impl->contextDocument/);
  assert.match(cpp,/emit accountDraftApplied\(\)/);
});
test('mobile bottom dock exposes saved casts without entering the account popup',()=>{
  const nav=code('ui/components/BottomNavigation.qml');
  const shell=code('ui/components/AppShell.qml');
  assert.match(nav,/objectName: "mobileBottomBookmarksButton"[\s\S]*?Theme\.iconBookmark[\s\S]*?root\.routeRequested\("bookmarks"\)/);
  assert.match(shell,/onRouteRequested: route => root\.openShellRoute\(route\)/);
  assert.match(shell,/BookmarksSurface \{[\s\S]*?presentationBridge\.bookmarkedCasts/);
});
