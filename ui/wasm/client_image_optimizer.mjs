// Genycaster Smart Media Pipeline: browser ingress.
// Compression is performed entirely on the user's device before the signed
// upload ticket is requested. Neither original bytes nor a decoded bitmap
// leave the browser; only the optimized Blob enters the Media Gateway.
const MIB=1024*1024;
export const MAX_SOURCE_IMAGE_BYTES=25*MIB;
export const MAX_IMAGE_PIXELS=32_000_000;

export async function optimizeBrowserImage(file,{
  maxBytes=2*MIB,signal,onProgress=()=>{},
  createBitmap=globalThis.createImageBitmap,
  createCanvas=()=>globalThis.document?.createElement('canvas'),
}={}){
  const aborted=()=>{if(signal?.aborted)throw new Error('Upload cancelled');};
  aborted();
  if(!file||!Number.isSafeInteger(file.size)||file.size<1
      ||file.size>MAX_SOURCE_IMAGE_BYTES)
    throw new Error('Source image exceeds the 25 MiB preparation safety limit.');
  if(maxBytes!==2*MIB && maxBytes!==5*MIB)
    throw new Error('Invalid optimized image size limit.');
  onProgress('preparing',0,file.size);
  // Never flatten animated GIFs; preserve their animation and explain
  // when a file cannot fit the account tier without destroying frames.
  // Animation is content, not an implementation detail. Browser canvas
  // snapshots only the first frame; never silently strip GIF/WebP/APNG
  // animations to save bandwidth. Detect WebP's VP8X ANIMATION bit and
  // PNG's acTL chunk before decoding (bounded to 64 KiB).
  let animated=file.type==='image/gif';
  if(!animated && (file.type==='image/webp'||file.type==='image/png')
       && typeof file.slice==='function'){
    const header=new Uint8Array(await file.slice(0,65536).arrayBuffer());
    aborted();
    if(file.type==='image/webp' && header.length>=21
      && String.fromCharCode(...header.subarray(0,4))==='RIFF'
      && String.fromCharCode(...header.subarray(8,12))==='WEBP'
      && String.fromCharCode(...header.subarray(12,16))==='VP8X')
      animated=(header[20]&0x02)!==0;
    if(file.type==='image/png' && header.length>=33){
      let offset=8;
      while(offset+12<=header.length){
        const length=((header[offset]<<24)>>>0)
          +(header[offset+1]<<16)+(header[offset+2]<<8)+header[offset+3];
        if(!Number.isSafeInteger(length)||length>16*1024*1024
           ||offset+length+12>header.length)break;
        const type=String.fromCharCode(...header.subarray(offset+4,offset+8));
        if(type==='acTL'){animated=true;break;}
        if(type==='IDAT')break;
        offset+=12+length;
      }
    }
  }
  if(animated){
    if(file.size>maxBytes)
      throw new Error('Animated image exceeds the account upload limit. Choose a smaller animation.');
    return {file,sourceBytes:file.size,optimized:false};
  }
  if(typeof createBitmap!=='function'){
    if(file.size>maxBytes)throw new Error('Local image compression is unavailable in this browser.');
    return {file,sourceBytes:file.size,optimized:false};
  }
  onProgress('compressing',0,file.size);
  let bitmap;
  try{
    bitmap=await createBitmap(file,{imageOrientation:'from-image'});
    aborted();
    if(!Number.isSafeInteger(bitmap.width)||!Number.isSafeInteger(bitmap.height)
      ||bitmap.width<1||bitmap.height<1
      ||bitmap.width*bitmap.height>MAX_IMAGE_PIXELS)
      throw new Error('Image dimensions exceed the safe 32 MP decoding limit.');
    const canvas=createCanvas();
    if(!canvas || typeof canvas.toBlob!=='function'){
      if(file.size>maxBytes)throw new Error('Local compression is unavailable for this image.');
      return {file,sourceBytes:file.size,optimized:false};
    }
    const maxEdge=maxBytes===5*MIB?3200:2560;
    const scale=Math.min(1,maxEdge/Math.max(bitmap.width,bitmap.height));
    const widths=[1,0.85,0.72];
    let best=null;
    for(const factor of widths){
      aborted();
      canvas.width=Math.max(1,Math.floor(bitmap.width*scale*factor));
      canvas.height=Math.max(1,Math.floor(bitmap.height*scale*factor));
      const ctx=canvas.getContext('2d',{alpha:true});
      if(!ctx)throw new Error('Image compression context is unavailable.');
      ctx.clearRect(0,0,canvas.width,canvas.height);
      ctx.drawImage(bitmap,0,0,canvas.width,canvas.height);
      for(const quality of [0.86,0.76,0.66,0.55]){
        aborted();
        const blob=await new Promise(resolve=>canvas.toBlob(resolve,'image/webp',quality));
        if(!blob || blob.type!=='image/webp')break;
        if(!best || blob.size<best.size)best=blob;
        if(blob.size<=maxBytes)break;
      }
      if(best && best.size<=maxBytes)break;
    }
    if(!best || best.size>maxBytes){
      if(file.size<=maxBytes)return {file,sourceBytes:file.size,optimized:false};
      throw new Error('Image remains above the account limit after local compression.');
    }
    // Preserve the original when it is already smaller and valid; do not
    // lossy-transcode an already efficient PNG/JPEG for a few saved bytes.
    if(file.size<=maxBytes && best.size>=file.size*0.92)
      return {file,sourceBytes:file.size,optimized:false};
    const base=String(file.name||'image').replace(/\.[^.]+$/,'').slice(0,110);
    const output=new File([best],base+'.webp',{type:'image/webp'});
    onProgress('compressing',output.size,file.size);
    return {file:output,sourceBytes:file.size,optimized:true};
  } finally {
    bitmap?.close?.();
  }
}
