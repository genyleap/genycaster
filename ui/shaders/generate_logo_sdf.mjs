import fs from "node:fs";
import { chromium } from "playwright";
const svg=fs.readFileSync(new URL("../wasm/genycaster-icon.svg",import.meta.url),"utf8");
const paths=[...svg.matchAll(/<path fill="(#[A-Fa-f0-9]{6})" d="([^"]+)"/g)];
const g=paths.find(x=>x[1]==="#FFFFFF")?.[2];
const arrow=paths.find(x=>x[1]==="#BEBEBE")?.[2];
if(!g||!arrow) throw new Error("Canonical SVG paths missing");
const browser=await chromium.launch({headless:true,args:["--no-sandbox"]});
console.log("VECTOR PATHS",g.length,arrow.length);
const page=await browser.newPage();
const result=await page.evaluate(async ({g,arrow})=>{
 const N=832, svgOrigin=48, pixelsPerSvg=2, maxDistance=48;
 function raster(path){
  const canvas=document.createElement("canvas");
  canvas.width=canvas.height=N;
  const ctx=canvas.getContext("2d",{willReadFrequently:true});
  ctx.scale(pixelsPerSvg,pixelsPerSvg);
  ctx.translate(-svgOrigin,-svgOrigin);
  ctx.fillStyle="#fff";
  ctx.fill(new Path2D(path));
  const rgba=ctx.getImageData(0,0,N,N).data;
  const binary=new Uint8Array(N*N);
  for(let i=0;i<binary.length;i++)binary[i]=rgba[i*4+3]>127?1:0;
  return binary;
 }
 function edt(mask,seed){
  const INF=1e15,f=new Float64Array(N),d=new Float64Array(N);
  const v=new Int32Array(N),z=new Float64Array(N+1);
  const vertical=new Float64Array(N*N),out=new Float32Array(N*N);
  function transform(){
   let k=0;
   v[0]=0;z[0]=-Infinity;z[1]=Infinity;
   for(let q=1;q<N;q++){
    let s=0;
    for(;;){
     const r=v[k];
     s=(f[q]+q*q-f[r]-r*r)/(2*(q-r));
     if(s>z[k]||k===0)break;
     k--;
    }
    k++;v[k]=q;z[k]=s;z[k+1]=Infinity;
   }
   k=0;
   for(let q=0;q<N;q++){
    while(z[k+1]<q)k++;
    const dx=q-v[k];d[q]=dx*dx+f[v[k]];
   }
  }
  for(let x=0;x<N;x++){
   for(let y=0;y<N;y++)f[y]=mask[y*N+x]===seed?0:INF;
   transform();
   for(let y=0;y<N;y++)vertical[y*N+x]=d[y];
  }
  for(let y=0;y<N;y++){
   for(let x=0;x<N;x++)f[x]=vertical[y*N+x];
   transform();
   for(let x=0;x<N;x++)out[y*N+x]=Math.sqrt(d[x]);
  }
  return out;
 }
 const gm=raster(g),am=raster(arrow);
 const gi=edt(gm,1),go=edt(gm,0),ai=edt(am,1),ao=edt(am,0);
 const canvas=document.createElement("canvas");
 canvas.width=canvas.height=N;
 const ctx=canvas.getContext("2d");
 const img=ctx.createImageData(N,N);
 for(let i=0;i<N*N;i++){
  const dsG=(gm[i]?-go[i]:gi[i])/pixelsPerSvg;
  const dsA=(am[i]?-ao[i]:ai[i])/pixelsPerSvg;
  const j=i*4;
  img.data[j]=Math.max(0,Math.min(255,Math.round(127.5+dsG*127.5/maxDistance)));
  img.data[j+1]=Math.max(0,Math.min(255,Math.round(127.5+dsA*127.5/maxDistance)));
  img.data[j+2]=0;img.data[j+3]=255;
 }
 ctx.putImageData(img,0,0);
 return {png:canvas.toDataURL("image/png"),width:N,
     gPixels:gm.reduce((a,b)=>a+b,0),arrowPixels:am.reduce((a,b)=>a+b,0)};
},{g,arrow});
const bytes=Buffer.from(result.png.split(",")[1],"base64");
const output=new URL("./genycaster_logo_sdf.png",import.meta.url);
fs.writeFileSync(output,bytes);
console.log("SVG MASTER SDF",result.width,result.gPixels,result.arrowPixels,bytes.length);
await browser.close();
