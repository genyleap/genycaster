import {createPrivateKey, sign} from 'node:crypto';

// Public directory is configuration, not an authorization or signer registry.
export const nodes = Object.freeze([
  {id:'directory-haatz',name:'Quilibrium Haatz',endpoint:'https://haatz.quilibrium.com',network:'hypersnap',contract:'hypersnap-v2',priority:100},
  {id:'directory-castrader',name:'Castrader',endpoint:'https://hypersnap.castrader.xyz',network:'hypersnap',contract:'hypersnap-v2',priority:100},
  {id:'directory-imgnai',name:'ImgNai',endpoint:'https://hypersnap.imgnai.com',network:'hypersnap',contract:'hypersnap-v2',priority:100},
  {id:'directory-genyleap',name:'Genyleap Snapchain',endpoint:'https://snapchain.genyleap.com',network:'farcaster',contract:'snapchain-v1',priority:100},
].map(node=>Object.freeze({...node,enabled:true})));
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
    }
    result.status=result.syncStatus==='lagging'?'degraded':'healthy';result.latencyMs=Math.max(0,now()-started);
  }catch(error){result.status=[401,402,403].includes(error.status)?'auth_required':'unreachable';}
  finally {result.checkedAt=new Date(now()).toISOString();}
  return result;
}
export function createNodeDirectory({fetchImpl=globalThis.fetch,now=Date.now,privateKeyPem=process.env.GENYCASTER_NODE_DIRECTORY_SIGNING_KEY,keyId=process.env.GENYCASTER_NODE_DIRECTORY_KEY_ID||'directory-v1'}={}) {
  const privateKey=privateKeyPem?createPrivateKey(privateKeyPem):null;
  if(privateKey&&privateKey.asymmetricKeyType!=='ed25519')throw Error('Node directory requires Ed25519');
  let cached,pending;
  async function status(){
    if(cached&&now()<cached.expiry)return cached.body;
    if(!pending)pending=Promise.all(nodes.map(node=>probeNode(node,{fetchImpl,now})))
      .then(rows=>{const body={schemaVersion:1,ttlSeconds:30,checkedAt:new Date(now()).toISOString(),nodes:rows};cached={body,expiry:now()+30000};return body;})
      .finally(()=>{pending=null;});
    return pending;
  }
  function manifest(network){
    const issuedAt=new Date(now()).toISOString();
    const payload={schemaVersion:1,revision:1,ttlSeconds:300,issuedAt,expiresAt:new Date(now()+300000).toISOString(),nodes:nodes.filter(n=>!network||n.network===network).map(n=>({...n,interfaces:[{network:n.network,contract:n.contract,access:['read'],verification:'required'}]}))};
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
    if(path===base){send(200,manifest(network));return true;}
    if(path===base+'/status'){const data=await status();send(200,{...data,nodes:data.nodes.filter(row=>!network||nodes.some(n=>n.id===row.id&&n.network===network))});return true;}
    const match=path.match(/^\/api\/v1\/nodes\/([a-z0-9-]+)(\/status)?$/);
    const node=match&&nodes.find(n=>n.id===match[1]);
    if(!node){send(404,{error:'not_found'});return true;}
    if(match[2]){const data=await status();send(200,data.nodes.find(n=>n.id===node.id));}
    else send(200,node);
    return true;
  };
}
