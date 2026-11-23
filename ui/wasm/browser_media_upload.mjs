// Browser-only, signer-authorized image uploads to the Genyleap Media Gateway.
// No custody phrase, Ed25519 private CryptoKey, or image bytes enter Qt/QML.
import { createBrowserSignerVault } from './secure_signer_vault.mjs';
import { deployment } from './deployment_config.mjs';

const gateway=deployment.browserGateway;
const encoder=new TextEncoder();
const allowedTypes=new Map([
    ['jpg','image/jpeg'],['jpeg','image/jpeg'],['png','image/png'],
    ['webp','image/webp'],['gif','image/gif'],['avif','image/avif'],
]);
const hex=bytes=>Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');
function ensureImage(file,maxImageBytes=2*1024*1024) {
    if(!file || !/^[a-zA-Z0-9][a-zA-Z0-9._ -]{0,125}\.[A-Za-z0-9]{2,5}$/.test(file.name||''))
        throw new Error('Choose an image file with a supported extension.');
    const extension=file.name.split('.').at(-1).toLowerCase();
    const mime=allowedTypes.get(extension);
    if(!mime || file.type && file.type!==mime)
        throw new Error('Unsupported image format. Choose JPG, PNG, WebP, GIF, or AVIF.');
    if(![2*1024*1024,5*1024*1024].includes(maxImageBytes))
        throw new Error('Invalid media upload limit.');
    if(!Number.isSafeInteger(file.size)||file.size<=0||file.size>maxImageBytes)
        throw new Error('Image exceeds the '+(maxImageBytes/1048576)+' MiB upload limit.');
    return mime;
}
async function responseJson(response) {
    const text=await response.text();
    if(text.length>65536)throw new Error('Media server response is too large.');
    let payload;
    try{payload=JSON.parse(text);}catch{throw new Error('Media server returned an invalid response.');}
    if(!response.ok)throw new Error('Media Gateway '+response.status+': '+String(payload.message||payload.error||'Upload failed').slice(0,150));
    return payload;
}
export async function uploadBrowserImage({
    file,fid,signal,onProgress=()=>{},fetchImpl=globalThis.fetch,
    crypto=globalThis.crypto,vault=createBrowserSignerVault(globalThis),
    optimizationMode='auto',maxImageBytes=2*1024*1024,
}={}) {
    const mime=ensureImage(file,maxImageBytes);
    if(!['auto','high','original'].includes(optimizationMode))
        throw new Error('Unsupported image optimization mode.');
    if(!/^[1-9][0-9]{0,19}$/.test(fid))throw new Error('Sign in to your Farcaster account first.');
    const key=await vault.load(fid);
    if(!key)throw new Error('Authorize your Farcaster publishing signer before uploading an image.');
    const signAndFetch=async(method,path,bytes,contentType,requestSignal=signal)=>{
        if(requestSignal?.aborted)throw new Error('Upload cancelled');
        const stamp=Math.floor(Date.now()/1000);
        const nonce=hex(crypto.getRandomValues(new Uint8Array(16)));
        const digest=hex(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)));
        const challenge=encoder.encode([
            'genycaster-media-upload-v1',method,path,fid,key.publicKeyHex,
            stamp,nonce,digest,
        ].join('\n'));
        const signed=await vault.signMediaChallenge(fid,challenge);
        if(signed.publicKeyHex!==key.publicKeyHex)throw new Error('Signer account changed');
        const response=await fetchImpl(gateway+path,{
            method,body:bytes,mode:'cors',credentials:'omit',redirect:'error',
            signal:requestSignal,
            headers:{
                'content-type':contentType,
                'x-genycaster-fid':fid,
                'x-genycaster-signer-key':key.publicKeyHex,
                'x-genycaster-timestamp':String(stamp),
                'x-genycaster-nonce':nonce,
                'x-genycaster-signature':signed.signature,
            },
        });
        return responseJson(response);
    };
    const json=(object)=>encoder.encode(JSON.stringify(object));
    let ticket=null;
    try{
        onProgress('authorizing',0,file.size);
        ticket=await signAndFetch('POST','/v1/uploads',json({
            fileName:file.name,contentType:mime,sizeBytes:file.size,
            optimizationMode,
        }),'application/json');
        if(ticket.mode!=='gateway-multipart'||!Number.isInteger(ticket.partCount)
           ||ticket.partCount<1||ticket.partCount>8
           ||!Number.isInteger(ticket.partSizeBytes)||ticket.partSizeBytes<1024
           ||typeof ticket.uploadId!=='string')
            throw new Error('Media Gateway returned an invalid upload ticket.');
        const parts=[];
        for(let n=1;n<=ticket.partCount;n++){
            const from=(n-1)*ticket.partSizeBytes;
            const to=Math.min(file.size,from+ticket.partSizeBytes);
            const bytes=new Uint8Array(await file.slice(from,to).arrayBuffer());
            onProgress('uploading',from,file.size);
            const path='/v1/uploads/'+encodeURIComponent(ticket.uploadId)+'/parts/'+n;
            const part=await signAndFetch('PUT',path,bytes,'application/octet-stream');
            if(part.partNumber!==n||typeof part.etag!=='string')
                throw new Error('Media Gateway did not acknowledge the uploaded part.');
            parts.push({partNumber:n,etag:part.etag});
            onProgress('uploading',to,file.size);
        }
        // The image is now fully transferred. The server owns decode,
        // optimization and the final QStorage write. Do not display a made-up
        // compression percentage while the signed complete call is pending.
        const serverOptimizes=(
            ticket.optimizationMode==='auto' || ticket.optimizationMode==='high');
        onProgress(serverOptimizes ? 'optimizing' : 'finalizing',
                   file.size,file.size);
        const completed=await signAndFetch('POST','/v1/uploads/complete',
            json({uploadId:ticket.uploadId,parts}),'application/json');
        if(completed.assetUrl!==ticket.assetUrl
           ||!completed.assetUrl.startsWith(deployment.mediaPublicOrigin+'/m/v1/')
           ||completed.sizeBytes!==file.size)
            throw new Error('Media Gateway returned an invalid media asset.');
        // The signed gateway reports source bytes separately from the
        // optimized bytes actually stored in QStorage.
        const storedSize=completed.storedSizeBytes;
        if(storedSize!==undefined && (!Number.isSafeInteger(storedSize)
           ||storedSize<=0||storedSize>file.size))
            throw new Error('Media Gateway returned an invalid optimized size.');
        if(serverOptimizes) onProgress('finalizing',file.size,file.size);
        return {...completed,sizeBytes:storedSize??completed.sizeBytes,
                sourceSizeBytes:file.size};
    }catch(error){
        // Abort is best-effort. A managed upload ticket is never logged.
        if(ticket?.uploadId) {
            // Network abort is not server-side abort: release reserved ingress
            // memory using a fresh, bounded request independent of the
            // already-aborted upload signal.
            const cleanup=new AbortController();
            const timer=setTimeout(()=>cleanup.abort(),6000);
            try{await signAndFetch('DELETE','/v1/uploads',
                json({uploadId:ticket.uploadId}),
                'application/json',cleanup.signal);}
            catch{/* ticket expires and the server sweeps it */}
            finally{clearTimeout(timer);}
        }
        throw error;
    }
}

export async function deleteBrowserAsset({
    fid,managementToken,fetchImpl=globalThis.fetch,crypto=globalThis.crypto,
    vault=createBrowserSignerVault(globalThis),
}={}) {
    if(!/^[1-9][0-9]{0,19}$/.test(String(fid||'')))
        throw new Error('Sign in to manage stored media.');
    if(typeof managementToken!=='string'||managementToken.length<16||managementToken.length>4096)
        throw new Error('Invalid managed media reference.');
    const key=await vault.load(fid);
    if(!key)throw new Error('A verified Farcaster signer is required for media cleanup.');
    const method='DELETE',path='/v1/assets';
    const bytes=encoder.encode(JSON.stringify({managementToken}));
    const stamp=Math.floor(Date.now()/1000);
    const nonce=hex(crypto.getRandomValues(new Uint8Array(16)));
    const digest=hex(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)));
    const challenge=encoder.encode([
        'genycaster-media-upload-v1',method,path,fid,key.publicKeyHex,
        stamp,nonce,digest,
    ].join('\n'));
    const signed=await vault.signMediaChallenge(fid,challenge);
    if(signed.publicKeyHex!==key.publicKeyHex)throw new Error('Signer account changed.');
    const response=await fetchImpl(gateway+path,{
        method,body:bytes,mode:'cors',credentials:'omit',redirect:'error',
        headers:{
            'content-type':'application/json',
            'x-genycaster-fid':fid,
            'x-genycaster-signer-key':key.publicKeyHex,
            'x-genycaster-timestamp':String(stamp),
            'x-genycaster-nonce':nonce,
            'x-genycaster-signature':signed.signature,
        },
    });
    // DELETE /v1/assets returns HTTP 204 with an empty body on success.
    // Do not attempt JSON parsing; that previously mislabeled a successful
    // object deletion as failed and kept cleanup retries forever.
    if(response.status===204)return {deleted:true};
    return responseJson(response);
}
