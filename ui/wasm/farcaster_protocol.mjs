// Browser-friendly port of the project's canonical Farcaster wire codec.
// Source of truth: src/protocol/{hash,message,key_authorization}.cpp
// This module contains no secrets, wallet authority, or network transport.
const iv = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
            0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
const perm = [2,6,3,10,7,0,4,13,1,11,12,5,9,14,15,8];
const chunkStart = 1, chunkEnd = 2, parent = 4, root = 8;
const encoder = new TextEncoder();
const farcasterEpoch = 1609459200;
const scopeNumbers = Object.freeze([1,2,3,4,5,6]);

function invariant(condition, message) {
    if (!condition) throw new Error('Farcaster protocol: ' + message);
}
function rotr(a,n) { return (a >>> n) | (a << (32-n)); }
function mix(s,a,b,c,d,x,y) {
    s[a] = (s[a] + s[b] + x) | 0;
    s[d] = rotr(s[d]^s[a],16);
    s[c] = (s[c]+s[d])|0;
    s[b] = rotr(s[b]^s[c],12);
    s[a] = (s[a]+s[b]+y)|0;
    s[d] = rotr(s[d]^s[a],8);
    s[c] = (s[c]+s[d])|0;
    s[b] = rotr(s[b]^s[c],7);
}
function compress(cv,words,counter,length,flags) {
    const s = [...cv, ...iv.slice(0,4),
        Number(BigInt(counter)&0xffffffffn),
        Number((BigInt(counter)>>32n)&0xffffffffn),length,flags];
    let m=words;
    for(let i=0;i<7;i++){
        mix(s,0,4,8,12,m[0],m[1]);
        mix(s,1,5,9,13,m[2],m[3]);
        mix(s,2,6,10,14,m[4],m[5]);
        mix(s,3,7,11,15,m[6],m[7]);
        mix(s,0,5,10,15,m[8],m[9]);
        mix(s,1,6,11,12,m[10],m[11]);
        mix(s,2,7,8,13,m[12],m[13]);
        mix(s,3,4,9,14,m[14],m[15]);
        if(i<6)m=perm.map(p=>m[p]);
    }
    return [...Array(8)].map((_,i)=>s[i]^s[i+8]).concat(
        [...Array(8)].map((_,i)=>s[i+8]^cv[i]));
}
function blockWords(data) {
    const result=Array(16).fill(0);
    for(let i=0;i<data.length;i++) result[i>>2] |= data[i]<<((i%4)*8);
    return result;
}
function chunkOutput(bytes,index) {
    let cv=iv.slice();
    const count=Math.max(1,Math.ceil(bytes.length/64));
    for(let i=0;i<count;i++){
        const block=bytes.subarray(i*64,Math.min((i+1)*64,bytes.length));
        const flags=(i===0?chunkStart:0)|(i===count-1?chunkEnd:0);
        const words=blockWords(block);
        if(i===count-1) return {cv,words,counter:BigInt(index),length:block.length,flags};
        cv=compress(cv,words,BigInt(index),block.length,flags).slice(0,8);
    }
}
function chaining(out) {
    return compress(out.cv,out.words,out.counter,out.length,out.flags).slice(0,8);
}
function tree(bytes,start,chunks) {
    if(chunks===1) return chunkOutput(bytes.subarray(start*1024,Math.min(bytes.length,(start+1)*1024)),start);
    let left=1;
    while(left*2<chunks)left*=2;
    const a=tree(bytes,start,left),b=tree(bytes,start+left,chunks-left);
    return {cv:iv.slice(),words:chaining(a).concat(chaining(b)),
            counter:0n,length:64,flags:parent};
}

export function blake3Digest20(bytes) {
    invariant(bytes instanceof Uint8Array, 'BLAKE3 requires bytes');
    const out=tree(bytes,0,Math.max(1,Math.ceil(bytes.length/1024)));
    const words=compress(out.cv,out.words,0n,out.length,out.flags|root);
    const result=new Uint8Array(20);
    for(let i=0;i<5;i++){
        result[i*4]=words[i]&255;
        result[i*4+1]=(words[i]>>>8)&255;
        result[i*4+2]=(words[i]>>>16)&255;
        result[i*4+3]=(words[i]>>>24)&255;
    }
    return result;
}

export function hex(bytes) {
    return [...bytes].map(x=>x.toString(16).padStart(2,'0')).join('');
}
export function fromHex(input,size) {
    invariant(typeof input==='string', 'hex string required');
    const text=input.replace(/^0x/i,'');
    invariant(text.length%2===0 && /^[0-9a-f]+$/i.test(text), 'invalid hexadecimal bytes');
    if(size!==undefined) invariant(text.length===size*2,'incorrect byte length');
    return Uint8Array.from(text.match(/../g).map(x=>parseInt(x,16)));
}
function concat(...inputs){
    const arrays=inputs.map(x=>typeof x==='string'?encoder.encode(x):x);
    const joined=new Uint8Array(arrays.reduce((n,x)=>n+x.length,0));
    let offset=0;
    for(const x of arrays){joined.set(x,offset);offset+=x.length;}
    return joined;
}
export function varint(value) {
    let v=BigInt(value);
    invariant(v>=0n && v<=0xffffffffffffffffn,'invalid unsigned 64-bit integer');
    const out=[];
    while(v>=128n){out.push(Number((v&127n)|128n));v>>=7n;}
    out.push(Number(v));
    return Uint8Array.from(out);
}
function fieldNum(tag,value){
    if(BigInt(value)===0n)return new Uint8Array();
    return concat(varint(tag*8),varint(value));
}
function fieldBytes(tag,value,emitEmpty=false){
    const bytes=typeof value==='string'?encoder.encode(value):value;
    if(!emitEmpty && !bytes.length)return new Uint8Array();
    return concat(varint(tag*8+2),varint(bytes.length),bytes);
}
export function farcasterTimestamp(unixSeconds=Math.floor(Date.now()/1000)){
    invariant(Number.isInteger(unixSeconds) && unixSeconds>=farcasterEpoch,'time before Farcaster epoch');
    const seconds=unixSeconds-farcasterEpoch;
    invariant(seconds<=0xffffffff,'Farcaster timestamp overflow');
    return seconds;
}
function fidString(value){
    const string=String(value);
    invariant(typeof value==='string' || (typeof value==='number' && Number.isSafeInteger(value)),
              'FID precision is unsafe');
    invariant(/^[1-9][0-9]{0,19}$/.test(string) && BigInt(string)<=0xffffffffffffffffn,
             'invalid FID');
    return string;
}
export function abiMetadata(fid,address,signature,deadline){
    const id=fidString(fid);
    const addr=fromHex(address,20);
    const sig=fromHex(signature,65);
    const word=value=>{
        const v=BigInt(value); invariant(v>=0n && v<(1n<<256n),'ABI uint256 overflow');
        return fromHex(v.toString(16).padStart(64,'0'),32);
    };
    const padded=new Uint8Array(96);padded.set(sig);
    return concat(word(32),word(id),new Uint8Array(12),addr,
                  word(128),word(deadline),word(65),padded);
}
export function keyAddTypedData({fid,publicKeyHex,scopes=scopeNumbers,ttl,nonce,deadline,
                                  requestDeadline=Math.floor(Date.now()/1000)+3600,requestFid=fid}){
    const key='0x'+hex(fromHex(publicKeyHex,32));
    const user=fidString(fid),requester=fidString(requestFid);
    invariant(scopes.length>0 && scopes.length<=16 && new Set(scopes).size===scopes.length &&
        scopes.every(x=>[1,2,3,4,5,6,7,8,11,12,13,14,15].includes(x)), 'invalid signer scopes');
    for(const [value,label,max] of [[ttl,'ttl',7776000],[nonce,'nonce',0xffffffff],
                                    [deadline,'deadline',0xffffffff]]){
        invariant(Number.isInteger(value) && value>0 && value<=max, 'invalid '+label);
    }
    invariant(Number.isSafeInteger(requestDeadline) && requestDeadline>Math.floor(Date.now()/1000),
              'invalid SignedKeyRequest deadline');
    // SignedKeyRequest is verified against the canonical Optimism validator.
    // KEY_ADD itself uses a separate Snapchain EIP-712 domain.
    const requestDomain={name:'Farcaster SignedKeyRequestValidator',version:'1',
        chainId:10,verifyingContract:'0x00000000fc700472606ed4fa22623acf62c60553'};
    const domain={name:'Farcaster KeyAdd',version:'1',chainId:10};
    const requestDomainFields=[{name:'name',type:'string'},{name:'version',type:'string'},
        {name:'chainId',type:'uint256'},{name:'verifyingContract',type:'address'}];
    const EIP712Domain=requestDomainFields.slice(0,3);
    const signedKeyRequest={
        types:{EIP712Domain:requestDomainFields,SignedKeyRequest:[
            {name:'requestFid',type:'uint256'},{name:'key',type:'bytes'},
            {name:'deadline',type:'uint256'}]},
        primaryType:'SignedKeyRequest',domain:requestDomain,
        message:{requestFid:requester,key,deadline:String(requestDeadline)}};
    const keyAdd={
        types:{EIP712Domain,KeyAdd:[
            {name:'fid',type:'uint256'},{name:'key',type:'bytes'},
            {name:'keyType',type:'uint32'},{name:'scopes',type:'uint32[]'},
            {name:'ttl',type:'uint32'},{name:'nonce',type:'uint32'},
            {name:'deadline',type:'uint256'}]},
        primaryType:'KeyAdd',domain,
        message:{fid:user,key,keyType:1,scopes,ttl,nonce,deadline:String(deadline)}};
    return {signedKeyRequest,keyAdd};
}
export function unsignedKeyAdd({
    fid,publicKeyHex,scopes=scopeNumbers,ttl,nonce,deadline,
    custodySignature,metadata, timestamp=farcasterTimestamp()
}){
    const id=fidString(fid),key=fromHex(publicKeyHex,32);
    const cust=fromHex(custodySignature,65);
    invariant(metadata instanceof Uint8Array && metadata.length && metadata.length%32===0,
              'invalid SignedKeyRequest metadata');
    invariant(scopes.length>0 && new Set(scopes).size===scopes.length,'duplicate/absent scopes');
    const body=concat(fieldBytes(1,key),fieldNum(2,1),fieldBytes(3,cust),
        fieldNum(4,deadline),fieldNum(5,nonce),fieldBytes(6,metadata),
        fieldNum(7,1),fieldBytes(9,concat(...scopes.map(varint))),fieldNum(10,ttl));
    const data=concat(fieldNum(1,16),fieldNum(2,id),fieldNum(3,timestamp),
        fieldNum(4,1),fieldBytes(19,body,true));
    return {data,hash:blake3Digest20(data),key};
}
export function unsignedCastAdd({
    fid,publicKeyHex,text,mentions=[],mentionPositions=[],embeds=[],parent=null,
    timestamp=farcasterTimestamp()
}){
    const id=fidString(fid),key=fromHex(publicKeyHex,32);
    const bodyText=encoder.encode(text);
    invariant(bodyText.length<=1024 && (bodyText.length>0||embeds.length>0),'invalid cast text');
    invariant(mentions.length===mentionPositions.length && mentions.length<=10
              && embeds.length<=2,'cast field limit exceeded');
    let body=new Uint8Array();
    const castId=ref=>concat(fieldNum(1,fidString(ref.fid)),fieldBytes(2,fromHex(ref.hash,20)));
    if(parent)body=concat(body,fieldBytes(3,castId(parent),true));
    if(mentions.length)body=concat(body,fieldBytes(2,concat(...mentions.map(x=>varint(fidString(x))))));
    body=concat(body,fieldBytes(4,bodyText));
    if(mentionPositions.length)body=concat(body,fieldBytes(5,concat(...mentionPositions.map(varint))));
    for(const embed of embeds){
        const bytes=embed.url?fieldBytes(1,embed.url):fieldBytes(2,castId(embed.cast),true);
        body=concat(body,fieldBytes(6,bytes,true));
    }
    if(bodyText.length>320)body=concat(body,fieldNum(8,1));
    const data=concat(fieldNum(1,1),fieldNum(2,id),fieldNum(3,timestamp),
        fieldNum(4,1),fieldBytes(5,body,true));
    return {data,hash:blake3Digest20(data),key};
}
export function sealMessage(unsigned,signature) {
    invariant(unsigned?.data instanceof Uint8Array && unsigned.hash?.length===20,'invalid message');
    const sig=signature instanceof Uint8Array?signature:fromHex(signature,64);
    invariant(sig.length===64 && unsigned.key?.length===32,'invalid Ed25519 signer');
    return concat(fieldBytes(2,unsigned.hash),fieldNum(3,1),
        fieldBytes(4,sig),fieldNum(5,1),fieldBytes(6,unsigned.key),
        fieldBytes(7,unsigned.data));
}
export const SOCIAL_SCOPES=scopeNumbers;

function encodeCastReference(reference) {
    return concat(fieldNum(1,fidString(reference.fid)),
        fieldBytes(2,fromHex(reference.hash,20)));
}
function unsignedSocial({fid,publicKeyHex,kind,bodyField,body,
                         timestamp=farcasterTimestamp()}){
    const key=fromHex(publicKeyHex,32);
    const data=concat(fieldNum(1,kind),fieldNum(2,fidString(fid)),
        fieldNum(3,timestamp),fieldNum(4,1),fieldBytes(bodyField,body,true));
    return {data,hash:blake3Digest20(data),key};
}
export function unsignedReaction({
    fid,publicKeyHex,target,reaction='like',active=true,
    timestamp=farcasterTimestamp()
}){
    invariant(reaction==='like'||reaction==='recast','unsupported reaction');
    const body=concat(fieldNum(1,reaction==='like'?1:2),
        fieldBytes(2,encodeCastReference(target),true));
    return unsignedSocial({fid,publicKeyHex,
        kind:active?3:4,bodyField:7,body,timestamp});
}
export function unsignedFollow({
    fid,publicKeyHex,targetFid,active=true,timestamp=farcasterTimestamp()
}){
    invariant(fidString(fid)!==fidString(targetFid),'cannot follow self');
    const body=concat(fieldBytes(1,'follow'),fieldNum(3,fidString(targetFid)));
    return unsignedSocial({fid,publicKeyHex,
        kind:active?5:6,bodyField:14,body,timestamp});
}
export function unsignedCastRemove({
    fid,publicKeyHex,target,timestamp=farcasterTimestamp()
}){
    invariant(fidString(target.fid)===fidString(fid),'cannot delete another author cast');
    return unsignedSocial({fid,publicKeyHex,kind:2,bodyField:6,
        body:fieldBytes(1,fromHex(target.hash,20)),timestamp});
}
export function unsignedKeyRemove({
    fid,publicKeyHex,nonce,deadline,timestamp=farcasterTimestamp()
}){
    const body=concat(fieldBytes(1,fromHex(publicKeyHex,32)),
        fieldNum(3,2),fieldNum(4,deadline),fieldNum(5,nonce));
    return unsignedSocial({fid,publicKeyHex,kind:17,bodyField:20,body,timestamp});
}
