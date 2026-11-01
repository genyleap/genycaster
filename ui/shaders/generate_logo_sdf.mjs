// Precision atlas for the EXACT official GenyCaster SVG.
// The union's signed distance is packed into RG as an *opaque 16-bit* value;
// B carries the arrow material mask. RGB + A=255 avoids Qt texture premultiply.
// A separate smooth normal atlas is generated directly from float distances.
import fs from "node:fs";
import { chromium } from "playwright";
const svg=fs.readFileSync(new URL("../wasm/genycaster-icon.svg",import.meta.url),"utf8");
const paths=[...svg.matchAll(/<path fill="(#[A-Fa-f0-9]{6})" d="([^"]+)"/g)];
const g=paths.find(x=>x[1]==="#FFFFFF")?.[2];
const arrow=paths.find(x=>x[1]==="#BEBEBE")?.[2];
if(!g||!arrow) throw new Error("Canonical SVG paths missing");
const browser=await chromium.launch({headless:true,args:["--no-sandbox"]});
try {
  const page=await browser.newPage();
  const result=await page.evaluate(({g,arrow})=>{
    const S=4, N=416*S, origin=48, maxDistance=32;
    function raster(path){
      const c=document.createElement("canvas"); c.width=c.height=N;
      const ctx=c.getContext("2d",{willReadFrequently:true});
      ctx.scale(S,S);ctx.translate(-origin,-origin);
      ctx.fillStyle="#fff";ctx.fill(new Path2D(path));
      const px=ctx.getImageData(0,0,N,N).data;
      const mask=new Uint8Array(N*N);
      for(let i=0;i<mask.length;i++)mask[i]=px[i*4+3]>127?1:0;
      return mask;
    }
    // Exact two-pass Euclidean distance transform. No marching squares
    // polygon reconstruction or manually approximated G/arrow contours.
    function edt(mask,seed){
      const INF=1e15, f=new Float64Array(N),d=new Float64Array(N);
      const v=new Int32Array(N),z=new Float64Array(N+1);
      const vertical=new Float64Array(N*N),out=new Float32Array(N*N);
      function transform(){
        let k=0; v[0]=0; z[0]=-Infinity;z[1]=Infinity;
        for(let q=1;q<N;q++){
          let s;
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
          const delta=q-v[k];d[q]=delta*delta+f[v[k]];
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
    const gm=raster(g), am=raster(arrow);
    const union=new Uint8Array(N*N);
    for(let i=0;i<union.length;i++)union[i]=gm[i]|am[i];
    const outside=edt(union,0),inside=edt(union,1);
    const arrowOutside=edt(am,0),arrowInside=edt(am,1);
    const signed=new Float32Array(N*N);
    const canvas=document.createElement("canvas");canvas.width=canvas.height=N;
    const ctx=canvas.getContext("2d");
    const img=ctx.createImageData(N,N);
    for(let i=0;i<signed.length;i++){
      // Half-texel boundary centering gives a symmetric signed field.
      const d=(union[i]?-outside[i]+.5:inside[i]-.5)/S;
      signed[i]=d;
      const arrowDist=(am[i]?-arrowOutside[i]+.5:arrowInside[i]-.5)/S;
      const packed=Math.max(0,Math.min(65535,Math.round(
        (Math.max(-maxDistance,Math.min(maxDistance,d))/maxDistance*.5+.5)*65535)));
      const j=i*4;
      img.data[j]=packed>>>8;
      img.data[j+1]=packed&255;
      img.data[j+2]=Math.round(127.5+Math.max(-7,Math.min(7,arrowDist))*127.5/7);
      img.data[j+3]=255;
    }
    ctx.putImageData(img,0,0);
    const normalCanvas=document.createElement("canvas");
    normalCanvas.width=normalCanvas.height=N;
    const nc=normalCanvas.getContext("2d");
    const ni=nc.createImageData(N,N);
    function sample(x,y){
      return signed[Math.max(0,Math.min(N-1,y))*N+
                    Math.max(0,Math.min(N-1,x))];
    }
    // Analytic contour normals: sample the SAME vector path commands from
    // the official SVG, orient each tangent against the union fill, and
    // find the closest segment in a spatial grid. This entirely avoids the
    // concentric EDT gradient bands on a grazing 3D side wall.
    function vectorSegments(path){
      const tokens=path.match(/[MLHVCZ]|-?\d*\.?\d+(?:e[-+]?\d+)?/gi)||[];
      const segments=[];
      let i=0, cmd="",x=0,y=0,startX=0,startY=0;
      function line(x2,y2){
        if(Math.hypot(x2-x,y2-y)>.0001)segments.push([x,y,x2,y2]);
        x=x2;y=y2;
      }
      while(i<tokens.length){
        if(/^[MLHVCZ]$/.test(tokens[i]))cmd=tokens[i++];
        const num=()=>Number(tokens[i++]);
        if(cmd==="M"){x=num();y=num();startX=x;startY=y;cmd="L";}
        else if(cmd==="H"){line(num(),y);}
        else if(cmd==="V"){line(x,num());}
        else if(cmd==="L"){line(num(),num());}
        else if(cmd==="C"){
          const x1=num(),y1=num(),x2=num(),y2=num(),x3=num(),y3=num();
          const ax=x,ay=y;
          let prevX=x,prevY=y;
          for(let step=1;step<=384;step++){
            const t=step/384,u=1-t;
            const px=u*u*u*ax+3*u*u*t*x1+3*u*t*t*x2+t*t*t*x3;
            const py=u*u*u*ay+3*u*u*t*y1+3*u*t*t*y2+t*t*t*y3;
            if(Math.hypot(px-prevX,py-prevY)>.0001)
              segments.push([prevX,prevY,px,py]);
            prevX=px;prevY=py;
          }
          x=x3;y=y3;
        }
        else if(cmd==="Z"){line(startX,startY);cmd="";}
        else throw Error("Unexpected SVG path command "+cmd);
      }
      return segments;
    }
    const cellSize=8, columns=Math.ceil(416/cellSize);
    const buckets=Array.from({length:columns*columns},()=>[]);
    const maskAt=(px,py)=>{
      const xx=Math.round((px-origin)*S),yy=Math.round((py-origin)*S);
      if(xx<0||xx>=N||yy<0||yy>=N)return 0;
      return union[yy*N+xx];
    };
    function addSegments(path){
      for(const [ax,ay,bx,by] of vectorSegments(path)){
        const dx=bx-ax,dy=by-ay,len=Math.hypot(dx,dy);
        if(len<1e-5)continue;
        let nx=-dy/len,ny=dx/len;
        const mx=(ax+bx)*.5,my=(ay+by)*.5;
        // Hide shared internal boundaries. The union remains a single solid.
        let one=maskAt(mx+nx*.8,my+ny*.8);
        let two=maskAt(mx-nx*.8,my-ny*.8);
        if(one===two){
          one=maskAt(mx+nx*1.5,my+ny*1.5);
          two=maskAt(mx-nx*1.5,my-ny*1.5);
        }
        if(one===two)continue;
        if(one===1){nx=-nx;ny=-ny;}
        const segment={ax,ay,dx,dy,inv:1/(dx*dx+dy*dy),nx,ny};
        const lowX=Math.max(0,Math.floor((Math.min(ax,bx)-8-origin)/cellSize));
        const highX=Math.min(columns-1,Math.floor((Math.max(ax,bx)+8-origin)/cellSize));
        const lowY=Math.max(0,Math.floor((Math.min(ay,by)-8-origin)/cellSize));
        const highY=Math.min(columns-1,Math.floor((Math.max(ay,by)+8-origin)/cellSize));
        for(let cy=lowY;cy<=highY;cy++)
          for(let cx=lowX;cx<=highX;cx++)
            buckets[cy*columns+cx].push(segment);
      }
    }
    addSegments(g);addSegments(arrow);
    for(let y=0;y<N;y++){
      const py=origin+(y+.5)/S;
      const cy=Math.max(0,Math.min(columns-1,Math.floor((py-origin)/cellSize)));
      for(let x=0;x<N;x++){
        const i=y*N+x;
        let nx=0,ny=0;
        if(Math.abs(signed[i])<7.0){
          const px=origin+(x+.5)/S;
          const cx=Math.max(0,Math.min(columns-1,Math.floor((px-origin)/cellSize)));
          const bucket=buckets[cy*columns+cx];
          let nearest=Infinity;
          for(const segment of bucket){
            const tx=px-segment.ax,ty=py-segment.ay;
            const t=Math.max(0,Math.min(1,
                              (tx*segment.dx+ty*segment.dy)*segment.inv));
            const ddx=tx-t*segment.dx,ddy=ty-t*segment.dy;
            const d2=ddx*ddx+ddy*ddy;
            if(d2<nearest){nearest=d2;nx=segment.nx;ny=segment.ny;}
          }
        }
        if(nx===0&&ny===0){
          let dx=0,dy=0;
          for(let k=-2;k<=2;k++){
            const w=k===0?3:Math.abs(k)===1?2:1;
            dx+=w*(sample(x+4,y+k)-sample(x-4,y+k));
            dy+=w*(sample(x+k,y+4)-sample(x+k,y-4));
          }
          const mag=Math.max(1e-6,Math.hypot(dx,dy));
          nx=dx/mag;ny=dy/mag;
        }
        const j=i*4;
        ni.data[j]=Math.round(127.5+127.5*nx);
        ni.data[j+1]=Math.round(127.5+127.5*ny);
        ni.data[j+2]=127;
        ni.data[j+3]=255;
      }
    }
    nc.putImageData(ni,0,0);
    return {png:canvas.toDataURL("image/png"),
      normalPng:normalCanvas.toDataURL("image/png"),width:N,
      gPixels:gm.reduce((a,b)=>a+b,0),arrowPixels:am.reduce((a,b)=>a+b,0)};
  },{g,arrow});
  const bytes=Buffer.from(result.png.split(",")[1],"base64");
  const normals=Buffer.from(result.normalPng.split(",")[1],"base64");
  fs.writeFileSync(new URL("./genycaster_logo_sdf.png",import.meta.url),bytes);
  fs.writeFileSync(new URL("./genycaster_logo_normals.png",import.meta.url),normals);
  console.log("CANONICAL 16-BIT SVG SDF",result.width,result.gPixels,result.arrowPixels,
              bytes.length,"NORMALS",normals.length);
}finally{await browser.close();}
