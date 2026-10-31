import * as THREE from "./assets/three.module.js";

// A single cohesive object: the exact curved G and directional cut of the
// shipped GenyCaster SVG, extruded with polished bevels. Nothing decorative
// floats beside the mark; this IS the brand, not a random abstract scene.
const canvas = document.getElementById("scene");
const rail = document.querySelector(".brand-rail");
const app = document.getElementById("app");
const reduced = window.matchMedia("(prefers-reduced-motion: reduce)");
let renderer, scene, camera, identity, probe, lights, raf=0;
let dark=app.classList.contains("dark");
let targetX=0,targetY=0, frameX=0,frameY=0, pulse=0;
let alive=true;
let lastTime=0;
let fallback;

const svgScale=1.76;
const px=x=>(x-256)*svgScale;
const py=y=>(256-y)*svgScale;
function move(s,x,y){s.moveTo(px(x),py(y));}
function line(s,x,y){s.lineTo(px(x),py(y));}
function bez(s,a,b,c,d,x,y){s.bezierCurveTo(px(a),py(b),px(c),py(d),px(x),py(y));}

function exactG(){
 const s=new THREE.Shape();
 move(s,366,204);
 bez(s,342,157,298,128,249,128);
 bez(s,178.30,128,121,185.30,121,256);
 bez(s,121,326.70,178.30,384,249,384);
 bez(s,294,384,331.8,365.4,354.3,349.5);
 line(s,322,315.2);
 bez(s,303.6,330.2,281.4,336,249,336);
 bez(s,204.82,336,169,300.18,169,256);
 bez(s,169,211.82,204.82,176,249,176);
 bez(s,278,176,299.5,185,315.5,204);
 s.closePath();
 return s;
}
function exactDirection(){
 const s=new THREE.Shape();
 move(s,254,231);line(s,376,231);
 bez(s,382.08,231,385,233.92,385,240);
 line(s,385,362);
 bez(s,385,367.7,383.1,367.25,379.4,363.3);
 bez(s,341.2,321.9,296.6,280.8,252.2,245.8);
 bez(s,245.6,240.8,247,231,254,231);
 s.closePath();
 return s;
}
function studioEnvironment(isDark){
 // Baked-in lighting layout: broad white softbox, cyan rim, narrow warm
 // reflection; generated locally so no images, network textures or timing.
 const c=document.createElement("canvas");c.width=768;c.height=384;
 const g=c.getContext("2d",{alpha:false});
 g.fillStyle=isDark?"#0b1b2b":"#b1ccd9";g.fillRect(0,0,c.width,c.height);
 function box(x,y,w,h,color){
   const grd=g.createRadialGradient(x,y,3,x,y,Math.max(w,h));
   grd.addColorStop(0,color);grd.addColorStop(0.5,color.replace(/,\s*([.\d]+)\)$/," ,0.32)"));grd.addColorStop(1,"rgba(0,0,0,0)");
   g.fillStyle=grd;g.fillRect(x-w,y-h,2*w,2*h);
 }
 box(160,128,115,190,"rgba(255,255,255,0.95)");
 box(375,105,220,110,"rgba(142,242,255,0.85)");
 box(614,169,90,240,"rgba(255,199,137,0.87)");
 box(460,320,200,90,"rgba(94,219,242,0.75)");
 const t=new THREE.CanvasTexture(c);
 t.mapping=THREE.EquirectangularReflectionMapping;
 t.colorSpace=THREE.SRGBColorSpace;
 return t;
}
function makeMaterial(isArrow){
 if(isArrow){
  return new THREE.MeshPhysicalMaterial({
   color:dark?"#38C6E3":"#1B84B0",metalness:dark?0.32:0.42,
   roughness:0.115,transmission:dark?0.12:0.06,
   thickness:6,ior:1.46,clearcoat:1,clearcoatRoughness:0.055,
   envMapIntensity:dark?2.2:1.6
  });
 }
 return new THREE.MeshPhysicalMaterial({
  color:dark?"#D0F2F7":"#7196B4",metalness:dark?0.44:0.53,
  roughness:0.14,transmission:dark?0.16:0.05,
  thickness:7,ior:1.45,clearcoat:1,clearcoatRoughness:0.06,
  envMapIntensity:dark?2.5:1.9
 });
}
function addPiece(shape,isArrow){
 const geo=new THREE.ExtrudeGeometry(shape,{
   depth:isArrow?23:30,steps:1,
   bevelEnabled:true,bevelThickness:5.5,bevelSize:4.6,
   bevelSegments:6,curveSegments:40
 });
 geo.computeVertexNormals();
 const mesh=new THREE.Mesh(geo,makeMaterial(isArrow));
 mesh.position.z=isArrow?36:0;
 mesh.castShadow=false;mesh.receiveShadow=false;
 mesh.frustumCulled=false;
 identity.add(mesh);
}
function buildIdentity(){
 identity=new THREE.Group();
 scene.add(identity);
 addPiece(exactG(),false);
 addPiece(exactDirection(),true);
 identity.rotation.set(-.06,-.12,-.015);
 identity.position.set(0,-7,0);
}
function configureEnvironment(){
 if(probe){probe.dispose();probe=null;}
 const texture=studioEnvironment(dark);
 const pmrem=new THREE.PMREMGenerator(renderer);
 probe=pmrem.fromEquirectangular(texture);
 pmrem.dispose();texture.dispose();
 scene.environment=probe.texture;
 renderer.toneMappingExposure=dark?1.48:1.28;
}
function configureTheme(){
 app.classList.toggle("dark",dark);
 app.classList.toggle("light",!dark);
 if(!renderer)return;
 configureEnvironment();
 const [g,arrow]=identity.children;
 g.material.dispose();arrow.material.dispose();
 g.material=makeMaterial(false);arrow.material=makeMaterial(true);
 lights.key.intensity=dark?3.2:2.4;
 lights.rim.intensity=dark?2.1:1.35;
 lights.warm.intensity=dark?2.0:1.1;
 lights.hemi.intensity=dark?1.35:1.45;
 invalidate();
}
function resize(){
 if(!renderer)return;
 const w=Math.max(1,canvas.clientWidth),h=Math.max(1,canvas.clientHeight);
 renderer.setSize(w,h,false);
 camera.aspect=w/h;
 // Hold the sculpture in the central safe area, with clear top lockup and
 // bottom footer at every responsive width. Never crop its extrema.
 const fov=35*Math.PI/180;
 const occupiedHeight=550;
 const distance=Math.max(1050,occupiedHeight/(2*Math.tan(fov/2)) * 1.45,
   (625/Math.max(.58,w/h))/(2*Math.tan(fov/2)));
 camera.position.set(0,0,distance);
 camera.lookAt(0,0,0);
 camera.updateProjectionMatrix();
 invalidate();
}
function invalidate(){if(!raf&&alive)raf=requestAnimationFrame(render)}
function render(t){
 raf=0;
 if(!renderer)return;
 const settled=reduced.matches;
 const friction=settled?1:.14;
 frameX+=( (settled?0:targetX)-frameX)*friction;
 frameY+=( (settled?0:targetY)-frameY)*friction;
 identity.rotation.y=-.12+frameX;
 identity.rotation.x=-.06+frameY;
 identity.rotation.z=-.015;
 if(pulse>0){
    pulse=Math.max(0,pulse-.07);
    identity.scale.setScalar(1+.014*Math.sin(Math.PI*pulse));
 }else identity.scale.setScalar(1);
 renderer.render(scene,camera);
 if(!settled&&(Math.abs(targetX-frameX)>.00035||Math.abs(targetY-frameY)>.00035||pulse>0))invalidate();
}
function createFallback(){
 const img=document.createElement("img");
 img.src="./assets/genycaster-icon.svg";
 img.alt="";
 img.className="brand-gpu-fallback";
 img.setAttribute("aria-hidden","true");
 rail.querySelector(".gpu-art").appendChild(img);
 fallback=img;
}
function init(){
 renderer=new THREE.WebGLRenderer({
   canvas,alpha:true,antialias:true,powerPreference:"high-performance"
 });
 renderer.setPixelRatio(Math.min(window.devicePixelRatio||1,1.7));
 renderer.outputColorSpace=THREE.SRGBColorSpace;
 renderer.toneMapping=THREE.ACESFilmicToneMapping;
 scene=new THREE.Scene();
 camera=new THREE.PerspectiveCamera(35,1,.1,4000);
 const hemi=new THREE.HemisphereLight("#e2ffff","#17415a",1.35);
 const key=new THREE.DirectionalLight("#e8faff",3.2);key.position.set(-350,540,680);
 const rim=new THREE.DirectionalLight("#64f6f1",2.1);rim.position.set(410,-230,-330);
 const warm=new THREE.DirectionalLight("#ffdbab",2.0);warm.position.set(490,360,470);
 [hemi,key,rim,warm].forEach(x=>scene.add(x));
 lights={hemi,key,rim,warm};
 buildIdentity();configureTheme();resize();
 canvas.addEventListener("pointermove",e=>{
    if(reduced.matches)return;
    const r=canvas.getBoundingClientRect();
    targetX=((e.clientX-r.left)/r.width-.5)*.17;
    targetY=((e.clientY-r.top)/r.height-.5)*.10;
    invalidate();
 });
 canvas.addEventListener("pointerleave",()=>{targetX=0;targetY=0;invalidate()});
 canvas.addEventListener("click",()=>{if(reduced.matches)return;pulse=1;invalidate()});
 canvas.addEventListener("webglcontextlost",e=>{
    e.preventDefault();alive=false;cancelAnimationFrame(raf);
    canvas.style.display="none";createFallback();
 },{passive:false});
 window.addEventListener("resize",resize);
 new ResizeObserver(resize).observe(rail);
 reduced.addEventListener?.("change",()=>{targetX=0;targetY=0;invalidate()});
 document.addEventListener("visibilitychange",()=>{
    alive=!document.hidden;
    if(alive)invalidate();
 });
}
function setTheme(value){
 dark=value;configureTheme();
}
try{init();}catch(e){
 console.warn("[GenyCaster Glass G Arc] WebGL unavailable; SVG fallback:",e);
 canvas.style.display="none";createFallback();
}
export {setTheme};
