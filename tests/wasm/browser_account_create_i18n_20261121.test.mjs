import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../../'+path,import.meta.url),'utf8');
test('shared setup uses the same translated labels for every supported locale',()=>{
 const qml=read('ui/components/setup/InitialSetup.qml');
 for(const locale of ['en_US','fa_IR','ar_SA','de_DE','es_ES','fr_FR','hi_IN','ja_JP','ko_KR','pt_BR','ru_RU','zh_CN']){
  const catalog=read('ui/i18n/qml_'+locale+'.ts');
  for(const key of ['backup','request','confirmed','wallet_recovery_title','show_recovery_words','wallet_copy_label','wallet_backup_ack','wallet_verify_start','browser_wallet_copy_success','browser_wallet_copy_warning','confirmation_pending_dont_retry','native_proof_working']){
   assert.ok(qml.includes('qsTrId("genycaster.setup.'+key+'")'),key);
   const message=catalog.split('id="genycaster.setup.'+key+'"')[1]?.split('</message>')[0];
   assert.ok(message,locale+': '+key);
   assert.match(message,/<translation>[^<]+<\/translation>/,locale+': '+key);
  }
 }
 assert.match(qml,/layoutDirection: Theme.uiDirection/);
});
