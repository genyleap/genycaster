import http from 'node:http';
import {readFileSync} from 'node:fs';
import {createNodeDirectory} from './node_directory.mjs';
const credentialDir=process.env.CREDENTIALS_DIRECTORY;
if(!credentialDir)throw Error('systemd credential directory required');
const handle=createNodeDirectory({privateKeyPem:readFileSync(credentialDir+'/directory-key.pem')});
const server=http.createServer(async(req,res)=>{
 res.setHeader('Content-Type','application/json; charset=utf-8');
 res.setHeader('Cache-Control','no-store');
 res.setHeader('Access-Control-Allow-Origin','*');
 res.setHeader('X-Content-Type-Options','nosniff');
 try {
  if(!await handle(req,res,new URL(req.url,'http://localhost')))
   res.writeHead(404).end(JSON.stringify({error:'not_found'}));
 }catch{
  if(!res.headersSent)res.writeHead(503);
  res.end();
 }
});
server.listen(8799,'127.0.0.1');
