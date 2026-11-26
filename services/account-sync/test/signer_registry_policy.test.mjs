import test from 'node:test';
import assert from 'node:assert/strict';
import {isActiveGaslessSigner} from '../../deployment_config.mjs';

const now=1_800_000_000, last=now-1609459200;
const active={ttl:600,lastUsedAt:last,expiresAt:last+600};
test('gasless registry validity uses computed expiry and canonical wire timestamps',()=>{
  assert.equal(isActiveGaslessSigner(active,now),true);
  assert.equal(isActiveGaslessSigner({...active,expiresAt:undefined},now),true);
  assert.equal(isActiveGaslessSigner({ttl:'600',lastUsedAt:String(last)},now),true);
  for(const row of [
    {},{...active,ttl:undefined},{...active,lastUsedAt:undefined},
    {...active,ttl:0},{...active,ttl:7776001},{...active,ttl:true},
    {...active,ttl:'0600'},{...active,lastUsedAt:1,expiresAt:last+600},
    {...active,lastUsedAt:last-600,expiresAt:last},
    {...active,lastUsedAt:last+301,expiresAt:last+901},
    {...active,expiresAt:now+600},{...active,expiresAt:1}
  ])assert.equal(isActiveGaslessSigner(row,now),false,JSON.stringify(row));
});
