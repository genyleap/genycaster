import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const source=readFileSync('ui/components/profile/StreakCard.qml','utf8');

test('Streak milestones, flame and leaderboard use retained QSG surface shapes',()=>{
  assert.doesNotMatch(source,/\bradius\s*:/);
  assert.ok((source.match(/Superellipse\s*\{/g)||[]).length>=13);
  assert.match(source,/id: badgeTile/);
  assert.match(source,/curveDepth: Theme\.curveControl/);
  assert.match(source,/fillColor: root\.inspectedMilestone === threshold \|\| badgeTile\.earned/);
  assert.match(source,/borderColor: root\.inspectedMilestone === threshold/);
  assert.match(source,/delegate: Superellipse\s*\{\s*required property var modelData/);
});

test('RTL progress fills retain shared capsule curvature and exact data positions',()=>{
  assert.equal((source.match(/curveDepth: parent\.curveDepth/g)||[]).length,2);
  assert.match(source,/x: Theme\.rtl \? parent\.width - width : 0/);
  assert.match(source,/width: parent\.width \* badgeTile\.completed/);
  assert.match(source,/Math\.max\(0, root\.current \/ Math\.max\(1, root\.nextMilestone\)\)/);
  assert.match(source,/fillColor: Theme\.streakTrack/);
  assert.match(source,/fillColor: Theme\.streak/);
});

test('Streak circular markers preserve access, conditional states and animation lifecycle',()=>{
  assert.match(source,/curveDepth: width \/ 2\s*\n\s*exponent: 2/);
  assert.match(source,/running: root\.flameMotion/);
  assert.match(source,/root\.current > 0 \? \(Theme\.dark \?/);
  assert.match(source,/Accessible\.name: root\.milestoneName/);
  assert.match(source,/fillColor: modelData\.future \? Theme\.inkSubtle/);
  assert.match(source,/borderWidth: modelData\.completed \? 2 : 1/);
});
