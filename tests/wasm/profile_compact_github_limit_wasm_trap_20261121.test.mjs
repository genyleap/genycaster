import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const read = p => readFileSync(new URL('../../' + p, import.meta.url), 'utf8');

test('small profile typography and badges retain a single natural-width flow', () => {
  const profile = read('ui/components/profile/ProfileSurface.qml');
  const pro = read('ui/components/profile/ProfileProBadge.qml');
  const stats = profile.slice(profile.indexOf('id: profileStatsFlow'), profile.indexOf('objectName: "profileOwnerActions"'));
  const bio = profile.slice(profile.indexOf('objectName: "profileSaveStatus"'), profile.indexOf('id: profileStatsFlow'));
  assert.match(bio, /textSize: root\.compact \? Theme\.typeLabelSize : Theme\.typeBodySize/);
  assert.match(bio, /textLineHeight: root\.compact \? Theme\.typeLabelLineHeight/);
  assert.match(stats, /id: followingCountButton[\s\S]*?font\.pixelSize: root\.compact \? Theme\.typeLabelSize/);
  assert.match(stats, /id: followersCountButton[\s\S]*?font\.pixelSize: root\.compact \? Theme\.typeLabelSize/);
  assert.match(stats, /objectName: "profileStreakAlignmentSpacer"[\s\S]*?visible: !root\.compact/);
  assert.match(stats, /width: root\.compact \? 0 : Math\.max\(0, profileStatsFlow\.width/);
  assert.match(stats, /id: profileBadgesRow[\s\S]*?height: Theme\.scaleUi\(root\.compact \? 28 : 32\)/);
  assert.match(stats, /id: profileStreakRing[\s\S]*?width: Theme\.scaleUi\(root\.compact \? 22 : 28\)/);
  assert.match(pro, /implicitHeight: Theme\.scaleUi\(compact \? 27 : 32\)/);
});

test('GitHub calendar does not redundantly fetch anonymous events for verified login', () => {
  const panel = read('ui/components/profile/GenyPulsePanel.qml');
  const activity = read('ui/app/social/activity.cpp');
  const header = read('ui/app/social/activity.hpp');
  const refresh = panel.slice(panel.indexOf('function refreshSources()'), panel.indexOf('function refreshGithubCalendar()'));
  assert.doesNotMatch(refresh, /requestGitHub\(verifiedGitHub\)/);
  assert.doesNotMatch(refresh, /requestGitHub\(""/);
  assert.match(refresh, /verifiedGitHub\.length===0 && isOwnProfile && githubLinked/);
  assert.match(refresh, /requestGitHubId\(githubAccountId\)/);
  assert.match(activity, /constexpr qint64 kGithubRateLimitCacheMs = 60 \* 60 \* 1000/);
  assert.match(activity, /githubCacheMs\(m_github\)/);
  assert.match(activity, /m_githubFetchedAt = QDateTime::currentMSecsSinceEpoch\(\)/);
  assert.match(header, /QPointer<QNetworkReply> m_githubResolveReply/);
});

test('WASM offset inspection reports imported function numbering and recursive calls', () => {
  const diag = read('ci/inspect_wasm_functions.py');
  assert.match(diag, /def import_function_count\(/);
  assert.match(diag, /function_index = imported_functions \+ i/);
  assert.match(diag, /recursive_call in bytes\(b\[p:p\+length\]\)/);
});
