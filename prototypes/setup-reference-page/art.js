import * as THREE from "./assets/three.module.js";

const canvas=document.getElementById("scene");
const rail=document.querySelector(".brand-rail");
const app=document.getElementById("app");
const prefersReducedMotion=window.matchMedia("(prefers-reduced-motion: reduce)");
let renderer,scene,camera,group,envTarget,raf=0,lastT=0,frameInvalidated=true,active=true;
const target={x:0,y:0},current={x:0,y:0};
let dark=app.classList.contains("dark"),gpuReady=false;
const materials=[];const disposables=[];

function createFilmMaterial({color,opacity=.70,glow=.3,shade=1}) {
  return new THREE.ShaderMaterial({
    uniforms:{
      tint:{value:new THREE.Color(color)},
      alpha:{value:opacity},
      luminous:{value:glow},
      shade:{value:shade}
    },
    side:THREE.DoubleSide,depthWrite:false,transparent:true,
    vertexShader:`
      varying vec3 vNormal;
      varying vec3 vView;
      varying vec3 vWorld;
      varying vec2 vUv;
      void main(){
        vec4 mv=modelViewMatrix*vec4(position,1.);
        vView=normalize(-mv.xyz);
        vNormal=normalize(normalMatrix*normal);
        vWorld=position;
        vUv=uv;
        gl_Position=projectionMatrix*mv;
      }`,
    fragmentShader:`
      precision highp float;
      uniform vec3 tint;
      uniform float alpha;
      uniform float luminous;
      uniform float shade;
      varying vec3 vNormal;
      varying vec3 vView;
      varying vec3 vWorld;
      varying vec2 vUv;
      void main(){
        vec3 n=normalize(vNormal);
        float f=pow(1.0-abs(dot(n,normalize(vView))),2.5);
        float g=pow(max(dot(reflect(normalize(vec3(-0.6,0.7,-0.8)),n),normalize(vView)),0.0),24.0);
        float w=pow(max(dot(reflect(normalize(vec3(0.9,0.14,-0.85)),n),normalize(vView)),0.0),48.0);
        float sheen=0.50+0.20*sin(vUv.y*3.14159+0.7)+0.17*sin(vUv.x*5.3);
        vec3 col=tint*(0.52+sheen*0.42)*shade;
        col+=vec3(0.65,1.0,.98)*(f*0.31+g*0.6)*luminous;
        col+=vec3(1.0,.74,.43)*w*.38*luminous;
        float opa=clamp(alpha*(0.53+0.45*sheen)+f*.18+g*.12,.02,.87);
        gl_FragColor=vec4(col,opa);
      }`
  });
}

function makeRibbon(points,widthBase,zDepth,material,{radius=3.5,color="#7df5ff",opacity=.85}={}){
  const curve=new THREE.CatmullRomCurve3(points.map(p=>new THREE.Vector3(...p)),false,"catmullrom",.46);
  const positions=[],uv=[],indices=[],edgeA=[],edgeB=[];
  const seg=140,slices=13;
  for(let i=0;i<=seg;i++){
    const t=i/seg,center=curve.getPoint(t),tangent=curve.getTangent(t).normalize();
    // Lateral tangent in XY plane forms an extruded broad folded glass surface.
    const side=new THREE.Vector3(-tangent.y,tangent.x,0).normalize();
    const flare=.82+.18*Math.sin(Math.PI*t),width=widthBase*flare;
    for(let j=0;j<=slices;j++){
      const v=j/slices,across=(v-.5)*width;
      const q=center.clone().addScaledVector(side,across);
      q.z+=zDepth*Math.sin(Math.PI*v)+14*Math.sin(t*4.2+v*2.5);
      positions.push(q.x,q.y,q.z);uv.push(v,t);
      if(j===0)edgeA.push(q.clone().add(new THREE.Vector3(0,0,3)));
      if(j===slices)edgeB.push(q.clone().add(new THREE.Vector3(0,0,3)));
    }
  }
  for(let i=0;i<seg;i++)for(let j=0;j<slices;j++){
    const a=i*(slices+1)+j,b=a+slices+1;
    indices.push(a,b,a+1,b,b+1,a+1);
  }
  const geo=new THREE.BufferGeometry();
  geo.setAttribute("position",new THREE.Float32BufferAttribute(positions,3));
  geo.setAttribute("uv",new THREE.Float32BufferAttribute(uv,2));
  geo.setIndex(indices);geo.computeVertexNormals();
  const surf=new THREE.Mesh(geo,material);surf.frustumCulled=false;
  group.add(surf);disposables.push(geo,material);
  const trace=(edge,r,op)=>{
    const g=new THREE.TubeGeometry(new THREE.CatmullRomCurve3(edge),135,r,6,false);
    const m=new THREE.MeshBasicMaterial({color,transparent:true,opacity:op,blending:THREE.AdditiveBlending,depthWrite:false,toneMapped:false});
    const obj=new THREE.Mesh(g,m);obj.frustumCulled=false;group.add(obj);disposables.push(g,m);
  };
  trace(edgeA,radius,opacity*.77);trace(edgeB,radius*.66,opacity*.57);
}

function addOrb(x,y,z,radius,color,alpha=1){
  const geo=new THREE.SphereGeometry(radius,46,34);
  const mat=new THREE.MeshPhysicalMaterial({
    color,roughness:.065,metalness:.03,
    transmission:.68,thickness:4,ior:1.45,clearcoat:1,clearcoatRoughness:.045,
    envMapIntensity:2.8,transparent:alpha<1,opacity:alpha
  });
  const m=new THREE.Mesh(geo,mat);m.position.set(x,y,z);group.add(m);disposables.push(geo,mat);
}

function buildScene(){
  scene=new THREE.Scene();group=new THREE.Group();scene.add(group);
  const width=Math.max(1,canvas.clientWidth),height=Math.max(1,canvas.clientHeight);
  camera=new THREE.PerspectiveCamera(42,width/height,.1,4000);
  camera.position.set(0,0,1080);camera.lookAt(0,0,0);
  const hemi=new THREE.HemisphereLight("#C7FAFF","#14537C",2.3);scene.add(hemi);
  const key=new THREE.DirectionalLight("#C1FFFF",6);key.position.set(-410,380,690);scene.add(key);
  const warm=new THREE.DirectionalLight("#FFD6A6",3.3);warm.position.set(480,220,470);scene.add(warm);
  const cyan=new THREE.PointLight("#54DDFA",24000,1200);cyan.position.set(-200,-130,540);scene.add(cyan);
  group.position.set(-15,-3,0);
  // The composition is deliberately one continuous visual family, not
  // isolated tubes, panels and disconnected objects.
  makeRibbon(
    [[-365,355,-140],[-286,215,-40],[-150,105,0],[5,75,90],[138,-10,90],[215,-160,-20],[265,-365,-130]],
    200,50,createFilmMaterial({color:"#2aaec6",opacity:.49,glow:.83}),
    {radius:2.5,color:"#4defff",opacity:.93}
  );
  makeRibbon(
    [[-410,85,120],[-315,45,170],[-135,-105,188],[75,-185,185],[248,-238,96],[330,-360,-40]],
    150,47,createFilmMaterial({color:"#59bde5",opacity:.65,glow:1.0}),
    {radius:3.8,color:"#81efff",opacity:.95}
  );
  makeRibbon(
    [[355,370,-80],[220,190,-10],[250,45,85],[340,-130,20],[380,-320,-60]],
    148,52,createFilmMaterial({color:"#bce5ff",opacity:.36,glow:.72}),
    {radius:1.8,color:"#effbff",opacity:.64}
  );
  addOrb(145,-105,238,119,"#7ebcfb",.93);
  addOrb(-230,-380,125,118,"#99d6fa",.75);
  invalidate();
}

function setTheme(value){
  dark=value;app.classList.toggle("dark",dark);app.classList.toggle("light",!dark);
  updateBackground();
  invalidate();
}
function updateBackground(){
  if(!scene)return;
  for(const l of scene.children.filter(x=>x.isLight)){
    if(l.isHemisphereLight)l.intensity=dark?2.3:2.1;
    if(l.isDirectionalLight)l.intensity=dark?6:4;
  }
  group.traverse(obj=>{
    if(!obj.isMesh)return;
    const m=obj.material;
    if(m.uniforms){
      m.uniforms.shade.value=dark?1:.82;
    } else if(m.isMeshPhysicalMaterial){
      m.envMapIntensity=dark?2.8:2;
    }
  });
}
function invalidate(){if(!raf&&active)raf=requestAnimationFrame(frame)}
function frame(time){
  raf=0;if(!renderer)return;
  const reduced=prefersReducedMotion.matches;
  if(!reduced){
    current.x+=(target.x-current.x)*.13;
    current.y+=(target.y-current.y)*.13;
  }else{current.x=0;current.y=0}
  group.rotation.y=current.x;
  group.rotation.x=current.y;
  renderer.render(scene,camera);
  if(!reduced && (Math.abs(current.x-target.x)>.00045||Math.abs(current.y-target.y)>.00045)) invalidate();
}
function resize(){
  if(!renderer)return;
  const width=Math.max(1,canvas.clientWidth),height=Math.max(1,canvas.clientHeight);
  renderer.setSize(width,height,false);
  camera.aspect=width/height;camera.updateProjectionMatrix();invalidate();
}
function init(){
  renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:true,powerPreference:"high-performance"});
  renderer.setPixelRatio(Math.min(1.5,devicePixelRatio||1));
  renderer.setSize(Math.max(1,canvas.clientWidth),Math.max(1,canvas.clientHeight),false);
  renderer.outputColorSpace=THREE.SRGBColorSpace;
  renderer.toneMapping=THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure=1.6;
  buildScene();
  updateBackground();
  resize();
  canvas.addEventListener("pointermove",e=>{
    if(prefersReducedMotion.matches)return;
    const rect=canvas.getBoundingClientRect();
    target.x=((e.clientX-rect.left)/rect.width-.5)*.23;
    target.y=((e.clientY-rect.top)/rect.height-.5)*.11;
    invalidate();
  });
  canvas.addEventListener("pointerleave",()=>{target.x=0;target.y=0;invalidate()});
  window.addEventListener("resize",resize);
  new ResizeObserver(resize).observe(rail);
  document.addEventListener("visibilitychange",()=>{active=!document.hidden;if(active)invalidate()});
  canvas.addEventListener("webglcontextlost",e=>{e.preventDefault();active=false;console.warn("WebGL context lost")},{passive:false});
}

try{init()}catch(error){
  // Accessible static CSS gradient remains visible on weak GPUs; the actual
  // setup controls do not depend on WebGL initialization.
  console.warn("[GenyCaster setup concept] GPU unavailable:",error.message);
  canvas.style.display="none";
}
export {setTheme};
