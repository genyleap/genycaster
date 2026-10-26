import test from 'node:test';
import assert from 'node:assert/strict';
import {createPgStore} from '../pg_store.mjs';

const owner={issuer:'https://openproof.genyleap.com',subject:'farcaster:123'};
const hash='0x'+'ab'.repeat(20);

function fixture() {
  const writes=[];
  const client={
    async query(sql,params=[]) {
      if (sql==='BEGIN'||sql==='COMMIT'||sql==='ROLLBACK')
        return {rows:[],rowCount:0};
      if (sql.includes('INSERT INTO bookmark_changes')) {
        writes.push({table:'history',params});
        return {rows:[{sequence:42}],rowCount:1};
      }
      if (sql.includes('INSERT INTO bookmarks')) {
        writes.push({table:'current',params});
        return {rows:[{cast_hash:hash.slice(2),is_deleted:params[3],
          changed_seq:params[4],priority:params[5],preview:params[6]}],
          rowCount:1};
      }
      throw new Error('Unexpected SQL in bookmark fixture');
    },
    release(){}
  };
  const pool={
    async query(sql) {
      assert.match(sql,/schema_migrations/);
      return {rows:[{version:3},{version:4},{version:5}]};
    },
    async connect(){return client;},
    async end(){}
  };
  return {writes,pool};
}

test('deleted bookmark passes SQL NULL (not JSONB scalar null) to both tables',async()=>{
  const {writes,pool}=fixture();
  const store=await createPgStore('test://synthetic',{poolFactory:()=>pool});
  const saved=await store.put('bookmarks',owner,{
    cast_hash:hash,is_deleted:true,priority:0,preview:null,
    mutation_id:'abcd1234-1234-4000-8000-123456789abc'
  });
  assert.equal(saved.is_deleted,true);
  assert.deepEqual(writes.map(row=>row.table),['history','current']);
  for(const write of writes) {
    assert.equal(write.params[6],null,
      'JSON.stringify(null) is the string "null" and violates the SQL preview CHECK');
  }
});

test('saved cast sends an actual JSON preview document to both tables',async()=>{
  const {writes,pool}=fixture();
  const store=await createPgStore('test://synthetic',{poolFactory:()=>pool});
  const preview={authorFid:123,text:'A saved cast'};
  await store.put('bookmarks',owner,{
    cast_hash:hash,is_deleted:false,priority:1,preview,
    mutation_id:'abcd1234-1234-4000-8000-123456789abd'
  });
  assert.deepEqual(writes.map(row=>JSON.parse(row.params[6])),[preview,preview]);
});
