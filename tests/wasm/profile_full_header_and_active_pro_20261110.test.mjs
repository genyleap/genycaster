import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const profile = readFileSync('ui/components/profile/ProfileSurface.qml','utf8');
const pro = readFileSync('ui/components/profile/GenyProDetailsDrawer.qml','utf8');

test('profile entry positions the full inline header at the viewport top', () => {
  assert.match(profile, /function positionReaderAtHeaderTop\(\)/);
  assert.match(profile, /profileList\.headerItem\.y/);
  assert.match(profile, /profileList\.contentY = Math\.min\(root\.readOriginY\(\), headerY\)/);
  assert.match(profile, /function scrollToBeginning\(\)[\s\S]*?root\.profileEntryResetActive = true[\s\S]*?root\.positionReaderAtHeaderTop\(\)/);
  assert.match(profile, /function positionNewProfileAtBeginning\(\)[\s\S]*?root\.positionReaderAtHeaderTop\(\)/);
  assert.match(profile, /onContentHeightChanged:[\s\S]*?if \(root\.profileEntryResetActive\) \{\s*resetPositionTimer\.restart\(\)/);
  assert.match(profile, /onMovementStarted: \{[\s\S]*?root\.finishProfileEntryReset\(\)/);
  assert.match(profile, /spacing: root\.compact && root\.isOwnProfile \? 0 : Theme\.space2/);
});

test('mobile Pro drawer shows brand icon and meaningful verified features', () => {
  assert.match(pro,/objectName: "genyProIdentityMark"/);
  assert.doesNotMatch(pro,/Superellipse\s*\{\s*visible: !root\.compact[\s\S]*?text: Theme\.iconSpark/);
  assert.match(pro,/objectName: "genyProActiveBenefits"/);
  assert.match(pro,/visible: root\.eligible && !root\.checkoutActive/);
  assert.match(pro,/text: qsTrId\("genycaster\.pro\.checkout_benefits"\)/);
  for (const feature of ['feature_miniapps_title','bookmark_collections','connections','market_pulse','feature_composer_title'])
    assert.match(pro,new RegExp('genycaster\\.pro\\.'+feature));
  assert.match(pro,/iconGlyph: Theme\.iconClose/);
  assert.match(pro,/objectName: "genyProDrawerScrollableDetails"/);
});
