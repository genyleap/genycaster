import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const qml=readFileSync(new URL('../../ui/components/ProfileSurface.qml',import.meta.url),'utf8');
const cpp=readFileSync(new URL('../../ui/mobile/openproof_connections_controller.cpp',import.meta.url),'utf8');
function extracted(begin,end) {
  const start=qml.indexOf(begin);
  const finish=qml.indexOf(end,start+begin.length);
  assert(start>=0 && finish>start);
  return qml.slice(start,finish);
}
const received=extracted('        function onCloudProfileExtrasReceived(item) {',
  '        function onCloudProfileExtrasSaved(item) {');
function runReceived({dirty,baseRevision,remote}) {
  const calls=[];
  const root={
    isOwnProfile:true,cloudLocalDirty:dirty,
    cloudAccountFlags:{dirty,baseRevision},
    profileExtras:{profession:'Founder',category:'Technology'},
    cloudProfilePayload:()=>({profession:'Founder',category:'Technology',links:[]}),
    preserveConflictingProfile:(item)=>calls.push(['backup',item.revision]),
    restoreCloudProfile:(item)=>calls.push(['restore',item.revision]),
    setCloudFlags:(value)=>calls.push(['dirty',value])
  };
  const openProofConnections={
    pushCloudProfileExtras:(item)=>calls.push(['write',item.profession])
  };
  runInNewContext(received+'\nonCloudProfileExtrasReceived(remote);',{
    root,openProofConnections,remote,Object,Number
  });
  return calls;
}
test('stale dirty desktop cache does not overwrite newer phone profile',()=>{
  assert.deepEqual(runReceived({
    dirty:true,baseRevision:undefined,
    remote:{revision:2,category:'Technology',profession:'Engineer',links:[]}
  }),[['backup',2],['restore',2]]);
  assert.deepEqual(runReceived({
    dirty:true,baseRevision:1,
    remote:{revision:2,category:'Technology',profession:'Engineer',links:[]}
  }),[['backup',2],['restore',2]]);
});
test('explicit unsent edit with an unchanged server CAS can upload',()=>{
  assert.deepEqual(runReceived({
    dirty:true,baseRevision:2,
    remote:{revision:2,category:'Technology',profession:'Engineer',links:[]}
  }),[['write','Founder']]);
});
test('a clean device always reads database profile and empty-account import is bounded',()=>{
  assert.deepEqual(runReceived({
    dirty:false,baseRevision:null,
    remote:{revision:2,category:'Technology',profession:'Engineer',links:[]}
  }),[['restore',2]]);
  assert.deepEqual(runReceived({dirty:true,baseRevision:null,remote:null}),
    [['write','Founder']]);
});
test('profile backup survives restoring server revision',()=>{
  const settings={perAccountJson:JSON.stringify({
    'farcaster:123':{dirty:true,localBackups:[{value:{profession:'Founder'}}]}
  })};
  const root={profileExtrasOwnerKey:'farcaster:123'};
  const snippet=extracted('    function setCloudFlags(dirty, baseRevision) {',
    '    function preserveConflictingProfile(item) {');
  runInNewContext(snippet+'\nsetCloudFlags(false,5);',{
    root,profileCloudSettings:settings,JSON
  });
  const flags=JSON.parse(settings.perAccountJson)['farcaster:123'];
  assert.equal(flags.dirty,false);
  assert.equal(flags.baseRevision,5);
  assert.equal(flags.localBackups[0].value.profession,'Founder');
});
test('rich draft conflict cannot silently overwrite newer server revisions',()=>{
  assert.match(cpp,/const bool staleDirty=dirty && remoteRevision>m_draftRevision/);
  assert.match(cpp,/conflict-device-backup/);
  assert.match(cpp,/!seen \|\| staleDirty \|\| \(differs && remoteRevision==m_draftRevision\)/);
  assert.match(cpp,/m_settings->setValue\(key\+QStringLiteral\("\/dirty"\),false\)/);
  assert.match(cpp,/remoteRevision==m_draftRevision[\s\S]*?!differs/);
  assert.match(cpp,/emit cloudDraftSnapshotReceived\(viewer,serialized,deleted,remoteRevision\)/);
});
