import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync, readdirSync} from 'node:fs';
import {join} from 'node:path';

const root=process.cwd();
const qml=relative=>readFileSync(join(root,'ui',relative),'utf8');
function allQml(relative) {
  const dir=join(root,'ui',relative);
  return readdirSync(dir,{withFileTypes:true}).flatMap(entry=>{
    const name=join(relative,entry.name);
    return entry.isDirectory()?allQml(name):
      entry.name.endsWith('.qml')?[name]:[];
  });
}

test('adaptive GenyDrawer mirrors side edges for RTL and LTR, preserving bottom sheets',()=>{
  const drawer=qml('components/controls/GenyDrawer.qml');
  assert.match(drawer,/edge:\s*bottomSheet\s*\?\s*Qt\.BottomEdge\s*:\s*\(Theme\.rtl\s*\?\s*Qt\.LeftEdge\s*:\s*Qt\.RightEdge\)/);
  assert.match(drawer,/property:\s*"position"/);
  assert.match(drawer,/control\.edge\s*!==\s*Qt\.RightEdge/);
  const files=[...allQml('components'),...allQml('app')];
  const owners=files.filter(file=>/\bGenyDrawer\s*\{/.test(qml(file)));
  assert.ok(owners.length>=11,'audit the full drawer family');
  for (const file of owners){
    if(file==='components/controls/GenyDrawer.qml')continue;
    if(file==='components/profile/GenyProDetailsDrawer.qml') {
      // Checkout uniquely opens on the physical RIGHT on desktop and fills
      // the screen on mobile; other drawers retain their RTL edge policy.
      assert.match(qml(file),/edge: root\.compact \? Qt\.BottomEdge : Qt\.RightEdge/);
      assert.match(qml(file),/fullScreen: root\.compact && root\.manualExpanded && root\.checkoutActive/);
      assert.match(qml(file),/root\.fullScreen \? parent\.height/);
      continue;
    }
    // No accidental physical side overrides in child instances. All desktop
    // sheets inherit one source of truth; the bottomSheet flag stays adaptive.
    assert.doesNotMatch(qml(file),/\bedge\s*:\s*[^\n]*(?:Qt\.(?:LeftEdge|RightEdge)|Theme\.rtl)/,
      file+' must not bypass GenyDrawer');
  }
});

test('profile editors, app details, account and settings inherit the shared edge',()=>{
  const profile=qml('components/profile/ProfileSurface.qml');
  for(const id of ['profileExtrasEditor','profileStatusEditor','profileLinksDrawer'])
    assert.match(profile,new RegExp('GenyDrawer\\s*\\{\\s*id:\\s*'+id+'\\b[\\s\\S]*?bottomSheet:\\s*mobilePresentation'));
  const apps=qml('components/apps/AppsSurface.qml');
  assert.match(apps,/id:\s*miniAppDetailDrawer[\s\S]*?bottomSheet:\s*mobilePresentation/);
  const account=qml('components/profile/ConnectionsPopup.qml');
  assert.match(account,/bottomSheet:\s*mobilePresentation/);
  for(const name of ['LocaleSettingsPopup','DataStoragePopup'])
    assert.match(qml('components/'+componentPath(name)+'.qml'),/bottomSheet:\s*(?:mobilePresentation|root\.mobilePresentation)/);
  const theme=qml('theme/Theme.qml');
  assert.match(theme,/uiDirection:\s*rtl\s*\?\s*Qt\.RightToLeft\s*:\s*Qt\.LeftToRight/);
  assert.match(profile,/layoutDirection:\s*Theme\.uiDirection/);
});

test('mobile bottom sheets remain bottom, updates open as centered popups',()=>{
  const drawer=qml('components/controls/GenyDrawer.qml');
  assert.match(drawer,/edge:\s*bottomSheet\s*\?\s*Qt\.BottomEdge/);
  assert.match(qml('components/navigation/SearchPalette.qml'),/edge:\s*Qt\.TopEdge/);
  assert.match(qml('components/settings/UpdateNoticeDrawer.qml'),/Popup \{/);
  assert.doesNotMatch(qml('components/settings/UpdateNoticeDrawer.qml'),/edge:\s*Qt\.TopEdge/);
  for (const name of ['MobileCreateMenu','ComposerDrawer','GenyAdaptiveDialog'])
    assert.match(qml('components/'+componentPath(name)+'.qml'),/bottomSheet:\s*true/);
});
