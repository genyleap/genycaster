import {createPrivateKey, sign} from 'node:crypto';
import {readFileSync} from 'node:fs';
import {isIP} from 'node:net';

// Public directory is configuration, not an authorization or signer registry.
// Operator-owned configuration; never accept node records from HTTP input.
export function loadDirectory(path=process.env.GENYCASTER_NODE_DIRECTORY_FILE || new URL('./nodes.json',import.meta.url)) {
  const bytes=readFileSync(path);
  if(bytes.length>65536)throw Error('directory config too large');
  const config=JSON.parse(bytes);
  if(!Number.isSafeInteger(config.revision)||config.revision<1||
     !Array.isArray(config.nodes)||!config.nodes.length||config.nodes.length>32)throw Error('invalid directory config');
  const ids=new Set(),origins=new Set();
  const entries=config.nodes.map(node=>{
    const {id,name,endpoint,network,contract,priority,enabled,registrationAllowed=false}=node;
    const url=new URL(endpoint);
    if(typeof id!=='string'||!/^directory-[a-zA-Z0-9_.-]{1,54}$/.test(id)||
       typeof name!=='string'||!name.length||name.length>100||
       !((network==='hypersnap'&&['hypersnap-v2','hypersnap-raw-v1'].includes(contract))||(network==='farcaster'&&contract==='snapchain-v1'))||
       url.protocol!=='https:'||url.origin!==endpoint||url.username||url.password||(url.port&&url.port!=='3381')||
       !url.hostname.includes('.')||url.hostname.endsWith('.local')||isIP(url.hostname)||
       !Number.isSafeInteger(priority)||priority<0||priority>10000||typeof enabled!=='boolean'||typeof registrationAllowed!=='boolean'||
       (registrationAllowed&&contract!=='hypersnap-v2')||
       ids.has(id)||origins.has(endpoint))throw Error('invalid directory node');
    ids.add(id);origins.add(endpoint);
    return Object.freeze({id,name,endpoint,network,contract,priority,enabled,registrationAllowed});
  });
  return {revision:config.revision,nodes:Object.freeze(entries)};
}
export const {nodes}=loadDirectory();
const maxBody=128*1024;
async function json(url,fetchImpl,timeoutMs) {
  const response=await fetchImpl(url,{method:'GET',redirect:'error',headers:{Accept:'application/json'},signal:AbortSignal.timeout(timeoutMs)});
  if(!response.ok) {
    const error=new Error('http');error.status=response.status;throw error;
  }
  if(!response.body || Number(response.headers.get('content-length')||0)>maxBody) throw Error('body');
  const reader=response.body.getReader();const parts=[];let size=0;
  try {
    while(true){const {done,value}=await reader.read();if(done)break;
      size+=value.byteLength;if(size>maxBody){await reader.cancel();throw Error('size');}parts.push(Buffer.from(value));}
    return JSON.parse(Buffer.concat(parts).toString('utf8'));
  } finally {reader.releaseLock();}
}
export async function probeNode(node,{fetchImpl=globalThis.fetch,timeoutMs=4000,now=Date.now}={}) {
  const started=now();
  const result={id:node.id,status:'unknown',latencyMs:null,syncStatus:'unknown',capabilities:{profileRead:'unknown',castRead:'unknown',write:'unknown',signerRegistry:'unknown',nativeRegistration:'unknown'}};
  try {
    if(node.contract==='hypersnap-v2'){
      const info=await json(node.endpoint+'/v1/info',fetchImpl,timeoutMs);
      if(typeof info?.version!=='string'||!Array.isArray(info.shardInfos)||!info.shardInfos.length){result.status='incompatible';return result;}
      result.version=info.version;result.numShards=Number.isInteger(info.numShards)?info.numShards:null;
      const profile=await json(node.endpoint+'/v2/farcaster/user?fid=3',fetchImpl,timeoutMs);
      if(String(profile?.user?.fid)!=='3'){result.status='incompatible';return result;}
      result.capabilities.profileRead='available';
      const delays=info.shardInfos.map(s=>s.blockDelay);
      if(delays.every(d=>Number.isInteger(d)&&d>=0)){
        result.syncStatus=delays.every(d=>d<=120)?'synced':'lagging';
      }
      const shard0=info.shardInfos.find(s=>s.shardId===0);
      result.shardZeroBlockDelay=Number.isInteger(shard0?.blockDelay)?shard0.blockDelay:null;
      // A GET only proves a usable challenge, never POST admission or custody.
      if(result.shardZeroBlockDelay!==null&&result.shardZeroBlockDelay<=120){
        try{
          const reg=await json(node.endpoint+'/v1/nativeRegistration',fetchImpl,timeoutMs);
          const h=reg?.anchorBlockHeight,n=reg?.nextBlock,w=reg?.anchorWindow,b=reg?.minDifficultyBits;
          if(reg?.active===true&&/^0x[0-9a-f]{64}$/i.test(reg.anchorBlockHash||'')&&
             Number.isSafeInteger(h)&&h>0&&Number.isSafeInteger(n)&&n>=h&&
             Number.isInteger(w)&&w>0&&n-h<=w&&Number.isInteger(b)&&b>=1&&b<=256&&
             /^[1-9][0-9]*$/.test(reg.firstNativeFid||''))
            result.capabilities.nativeRegistration='preflight_available';
        }catch{}
      }
    } else {
      const casts=await json(node.endpoint+'/v1/castsByFid?fid=3&pageSize=1&reverse=true',fetchImpl,timeoutMs);
      const profile=await json(node.endpoint+'/v1/userDataByFid?fid=3&pageSize=100',fetchImpl,timeoutMs);
      if(!Array.isArray(casts?.messages)||!Array.isArray(profile?.messages)){result.status='incompatible';return result;}
      result.capabilities.profileRead='available';result.capabilities.castRead='available';
      try {
        const info=await json(node.endpoint+'/v1/info',fetchImpl,timeoutMs);
        if(typeof info?.version==='string')result.version=info.version;
        const delays=info?.shardInfos?.map(s=>s.blockDelay);
        if(delays?.length&&delays.every(d=>Number.isInteger(d)&&d>=0))
          result.syncStatus=delays.every(d=>d<=120)?'synced':'lagging';
      }catch{}
    }
    result.status=result.syncStatus==='lagging'?'degraded':'healthy';result.latencyMs=Math.max(0,now()-started);
  }catch(error){result.status=[401,402,403].includes(error.status)?'auth_required':'unreachable';}
  finally {result.checkedAt=new Date(now()).toISOString();}
  return result;
}
export function createNodeDirectory({fetchImpl=globalThis.fetch,now=Date.now,privateKeyPem=process.env.GENYCASTER_NODE_DIRECTORY_SIGNING_KEY,keyId=process.env.GENYCASTER_NODE_DIRECTORY_KEY_ID||'directory-v1',directory=loadDirectory()}={}) {
  const {revision,nodes}=directory;
  const privateKey=privateKeyPem?createPrivateKey(privateKeyPem):null;
  if(privateKey&&privateKey.asymmetricKeyType!=='ed25519')throw Error('Node directory requires Ed25519');
  let cached,pending;
  async function status(){
    if(cached&&now()<cached.expiry)return cached.body;
    if(!pending)pending=Promise.all(nodes.map(node=>node.enabled ? probeNode(node,{fetchImpl,now}) : {id:node.id,status:'maintenance',syncStatus:'unknown',latencyMs:null,checkedAt:new Date(now()).toISOString()}))
      .then(rows=>{const body={schemaVersion:1,ttlSeconds:30,checkedAt:new Date(now()).toISOString(),nodes:rows};cached={body,expiry:now()+30000};return body;})
      .finally(()=>{pending=null;});
    return pending;
  }
  function manifest(network,extended){
    const issuedAt=new Date(now()).toISOString();
    const payload={schemaVersion:1,revision,ttlSeconds:300,issuedAt,expiresAt:new Date(now()+300000).toISOString(),nodes:nodes.filter(n=>(!network||n.network===network)&&(extended||(n.contract!=='hypersnap-raw-v1'&&!new URL(n.endpoint).port))).map(n=>({...n,interfaces:[{network:n.network,contract:n.contract,access:['read'],verification:'required'}]}))};
    const bytes=Buffer.from(JSON.stringify(payload),'utf8');
    return {...payload,signature:privateKey?{algorithm:'Ed25519',keyId,payload:bytes.toString('base64url'),value:sign(null,bytes,privateKey).toString('base64url')}:null};
  }
  return async function handle(request,response,url){
    const path=url.pathname,base='/api/v1/nodes';
    if(path!==base&&!path.startsWith(base+'/'))return false;
    const send=(code,body)=>{const data=JSON.stringify(body);response.setHeader('Content-Length',Buffer.byteLength(data));response.writeHead(code);response.end(request.method==='HEAD'?undefined:data);};
    if(!['GET','HEAD'].includes(request.method)){response.setHeader('Allow','GET, HEAD');send(405,{error:'method_not_allowed'});return true;}
    const network=url.searchParams.get('network');
    if(network&&!['hypersnap','farcaster'].includes(network)){send(400,{error:'invalid_network'});return true;}
    if(path===base){send(200,manifest(network,url.searchParams.get('format')==='2'));return true;}
    if(path===base+'/status'){const data=await status();send(200,{...data,nodes:data.nodes.filter(row=>!network||nodes.some(n=>n.id===row.id&&n.network===network))});return true;}
    const match=path.match(/^\/api\/v1\/nodes\/([a-zA-Z0-9_.-]+)(\/status)?$/);
    const node=match&&nodes.find(n=>n.id===match[1]);
    if(!node){send(404,{error:'not_found'});return true;}
    if(match[2]){const data=await status();send(200,data.nodes.find(n=>n.id===node.id));}
    else send(200,node);
    return true;
  };
}
