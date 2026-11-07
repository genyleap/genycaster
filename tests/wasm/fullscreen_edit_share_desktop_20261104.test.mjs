import { componentPath } from './ui_component_paths.mjs';
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=(name)=>readFileSync(new URL('../../ui/components/' + componentPath(name)+'.qml',import.meta.url),'utf8');

test('shared Drawer contract has a handle only on short, swipeable bottom sheets',()=>{
 const src=read('GenyDrawer');
 assert.match(src,/property bool showHandle: bottomSheet && !fullScreen/);
 assert.match(src,/curveDepth: control\.fullScreen \? 0/);
 assert.match(src,/readonly property int sheetTopInset: showHandle \? Theme\.space5 : 0/);
 assert.match(src,/duration: control\.sheetMotionDuration/);
});
test('profile edit and status use genuinely fullscreen mobile forms with exit controls',()=>{
 const q=read('ProfileSurface');
 for(const id of ['profileExtrasEditor','profileStatusEditor']){
  const s=q.slice(q.indexOf('id: '+id));
  assert.match(s,/fullScreen: mobilePresentation/);
  assert.match(s,/showHandle: false/);
  assert.match(s,/interactive: false/);
  assert.match(s,/height: parent \? parent\.height/);
  assert.match(s,/sheetPresentation: false/);
 }
 for(const id of ['profileEditorScroll','statusFormScroll'])
  assert.match(q,new RegExp('id: '+id));
});
test('connections and activity inspector remain fully scrollable and occupy viewport',()=>{
 const connections=read('AccountConnectionsPopup');
 assert.match(connections,/fullScreen: mobilePresentation/);
 assert.match(connections,/showHandle: false/);
 assert.match(connections,/height: parent \? parent\.height/);
 const profile=read('ProfileSurface');
 const activity=profile.slice(profile.indexOf('id: activityDrawer'),profile.indexOf('id: streakDrawer'));
 assert.match(activity,/fullScreen: mobilePresentation/);
 assert.match(activity,/showHandle: false/);
 assert.match(activity,/height: parent \? parent\.height/);
 assert.match(activity,/objectName: "profileActivityDetailsScroll"/);
});
test('Share is a popup on a desktop viewport even when CastItem is compact',()=>{
 const share=read('EngagementBar');
 assert.match(share,/Overlay\.overlay\.width < Theme\.breakpointMobile\s*\? mobileCastShareSheet : desktopCastSharePopup/);
 assert.match(share,/if \(overlay\.width >= Theme\.breakpointMobile\)/);
 assert.match(share,/id: desktopCastSharePopup[\s\S]*?Popup \{/);
 assert.match(share,/id: mobileCastShareSheet[\s\S]*?GenyDrawer \{/);
});
