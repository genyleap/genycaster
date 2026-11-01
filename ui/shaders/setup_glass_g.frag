#version 440
// The authoritative front silhouette is generated directly from
// ui/wasm/genycaster-icon.svg. RG: 16-bit union signed distance.
// B: arrow-only material boundary; A is permanently opaque.
// The normals atlas derives from unquantized float distances.
// Everything is GPU raymarched through the portable Qt ShaderEffect pipeline.
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform Params {
 mat4 qt_Matrix;
 float qt_Opacity;
 vec2 paneSize;
 vec2 pointer;
 float darkMode;
 float pulseValue;
 float orbitYaw;
 float dragYaw;
 float dragPitch;
 float viewZoom;
} params;
layout(binding=1) uniform sampler2D logoDistanceMap;
layout(binding=2) uniform sampler2D logoNormalMap;
const float PI=3.14159265359;

vec3 objectSpace(vec3 p,vec2 rot) {
 float c=cos(rot.x),s=sin(rot.x);
 p=vec3(c*p.x-s*p.z,p.y,s*p.x+c*p.z);
 c=cos(rot.y);s=sin(rot.y);
 return vec3(p.x,c*p.y+s*p.z,-s*p.y+c*p.z);
}
vec2 canonicalDistance(vec2 xy) {
 // SVG viewBox 48 48 416 416, centered on 256,256.
 vec2 svg=vec2(256.0+xy.x*220.0,256.0-xy.y*220.0);
 vec2 uv=(svg-vec2(48.0))/416.0;
 vec2 bounded=clamp(uv,vec2(.001),vec2(.999));
 vec3 atlas=texture(logoDistanceMap,bounded).rgb;
 // Preserve 16-bit distance precision across interpolation. Quantization
 // falls below a tenth of a pixel even on oblique extruded surfaces.
 float packed=atlas.r*65280.0+atlas.g*255.0;
 float distance=(packed/65535.0*2.0-1.0)*32.0/220.0;
 float arrowDistance=(atlas.b*2.0-1.0)*7.0/220.0;
 if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0)))) {
  distance+=length((uv-bounded)*416.0/220.0);
 }
 return vec2(distance,arrowDistance);
}
vec2 logo(vec3 p) {
 vec2 contour=canonicalDistance(p.xy);
 // One *continuous* 3D solid for the G plus arrow: the original
 // exact front outline, no hidden join, identical rear/cap bevel.
 const float halfDepth=.112;
 const float bevel=.007;
 float z=abs(p.z)-halfDepth;
 vec2 q=vec2(contour.x+bevel,z+bevel);
 float distance=length(max(q,0.0))+min(max(q.x,q.y),0.0)-bevel;
 float arrow=1.0-smoothstep(-.005,.005,contour.y);
 return vec2(distance,arrow);
}
vec2 scene(vec3 p,vec2 angles){return logo(objectSpace(p,angles));}

vec3 worldNormal(vec3 n,vec2 rot) {
 // Inverse of objectSpace's yaw then pitch transform.
 float c=cos(rot.y),s=sin(rot.y);
 vec3 q=vec3(n.x,c*n.y-s*n.z,s*n.y+c*n.z);
 c=cos(rot.x);s=sin(rot.x);
 return vec3(c*q.x+s*q.z,q.y,-s*q.x+c*q.z);
}
vec3 normalAt(vec3 p,vec2 angles) {
 vec3 local=objectSpace(p,angles);
 vec2 svg=vec2(256.0+local.x*220.0,256.0-local.y*220.0);
 vec2 uv=clamp((svg-vec2(48.0))/416.0,vec2(.001),vec2(.999));
 // Precomputed 2D normals from the float SVG SDF are smooth even at
 // grazing angles; 8-bit raymarch distance bands never reach lighting.
 vec2 g=texture(logoNormalMap,uv).rg*2.0-1.0;
 vec3 side=normalize(vec3(g.x,-g.y,0.0));
 vec3 cap=vec3(0.,0.,local.z>=0.0?1.0:-1.0);
 float capWeight=smoothstep(.101,.109,abs(local.z));
 return normalize(worldNormal(normalize(mix(side,cap,capWeight)),angles));
}

float hash21(vec2 p){
 p=fract(p*vec2(123.34,456.21));
 p+=dot(p,p+45.32);
 return fract(p.x*p.y);
}
vec3 background(vec2 screen,vec2 uv,bool dark){
 // Continuous photographic cyclorama. The logo is suspended, so do NOT
 // project a duplicate silhouette onto an imaginary vertical wall.
 // Refined atmospheric brand rail: cool porcelain/light sapphire in
 // light mode and graphite/blue slate in dark mode. Three broad softboxes
 // create depth without fake projected silhouettes or hard floor lines.
 vec3 top=dark?vec3(.018,.035,.061):vec3(.795,.861,.936);
 vec3 bottom=dark?vec3(.027,.060,.095):vec3(.666,.776,.877);
 vec3 col=mix(top,bottom,smoothstep(.04,.97,uv.y));
 vec2 key=(screen-vec2(-.38,.34))*vec2(.78,.84);
 float softbox=exp(-dot(key,key)*1.28);
 col+=(dark?vec3(.054,.080,.122):vec3(.110,.115,.125))*softbox;
 vec2 fill=(screen-vec2(.79,-.29))*vec2(1.65,.97);
 float edgeFill=exp(-dot(fill,fill)*1.85);
 col+=(dark?vec3(.030,.071,.104):vec3(.065,.083,.104))*edgeFill;
 vec2 backgroundArc=(screen-vec2(-.15,.06))*vec2(.88,1.12);
 float halo=exp(-dot(backgroundArc,backgroundArc)*.96);
 col+=(dark?vec3(.022,.035,.054):vec3(.023,.034,.056))*halo;
 // Restrained optical falloff anchors the sculpture without a shadow
 // that resembles a duplicate of its SVG silhouette.
 vec2 ambient=(screen-vec2(.09,-.12))*vec2(.84,1.32);
 float shadow=exp(-dot(ambient,ambient)*2.3);
 col*=1.0-(dark?.027:.036)*shadow;
 col*=1.0-.088*smoothstep(.86,2.55,length(screen*vec2(.84,.91)));
 float grain=hash21(floor(uv*params.paneSize))-.5;
 col+=grain*(dark?.0007:.0005);
 return max(col,vec3(0.));
}

// Analytic HDR-looking light cards give a convincing surface response
// without cubemaps, network assets or nonportable GPU features.
vec3 studioEnv(vec3 r,bool dark) {
 float sky=smoothstep(-.9,.9,r.y);
 vec3 env=mix(
   dark?vec3(.015,.031,.052):vec3(.17,.23,.29),
   dark?vec3(.105,.165,.224):vec3(.54,.64,.72),sky);
 float stripA=exp(-pow((r.x+.34)*11.,2.))
             *smoothstep(-.60,-.09,r.y);
 float stripB=exp(-pow((r.x-.54)*16.,2.))
             *smoothstep(-.64,.19,r.y);
 float softbox=exp(-pow((r.y-.84)*6.,2.))
              *smoothstep(-.8,.5,r.z);
 env+=vec3(.90,.94,1.0)*(.85*stripA+.54*stripB+.35*softbox);
 float amber=exp(-pow((r.x-.88)*10.,2.)) * smoothstep(-.1,.7,r.y);
 env+=vec3(.94,.58,.34)*amber*.36;
 return env;
}
vec3 materialAt(vec3 p,vec3 ray,vec2 rotation,float arrow,bool dark){
 vec3 n=normalAt(p,rotation);
 vec3 v=normalize(-ray);
 float nv=clamp(dot(n,v),.001,1.);
 vec3 r=reflect(-v,n);
 // Two subtly different anodized finishes on the *same* extruded solid:
 // cool satin silver for G and darker titanium for the arrow.
 vec3 silver=dark?vec3(.73,.80,.88):vec3(.51,.62,.73);
 vec3 titanium=dark?vec3(.31,.37,.45):vec3(.26,.33,.41);
 vec3 albedo=mix(silver,titanium,arrow);
 // Broader, lower-energy highlights are closer to satin metal than chrome.
 float roughness=mix(.34,.40,arrow);
 float a=roughness*roughness;
 float a2=a*a;
 vec3 lightDir=normalize(vec3(-.54,.80,.66));
 vec3 h=normalize(lightDir+v);
 float nl=max(dot(n,lightDir),0.);
 float nh=max(dot(n,h),0.);
 float vh=max(dot(v,h),0.);
 float denominator=nh*nh*(a2-1.)+1.;
 float D=a2/max(PI*denominator*denominator,.0001);
 float k=(roughness+1.)*(roughness+1.)/8.;
 float Gv=nv/(nv*(1.-k)+k);
 float Gl=nl/(nl*(1.-k)+k);
 vec3 f0=mix(vec3(.65,.72,.79),vec3(.42,.48,.55),arrow);
 vec3 F=f0+(1.-f0)*pow(1.-vh,5.);
 vec3 direct=(D*Gv*Gl*F)/(4.*max(nv,.01));
 vec3 env=studioEnv(r,dark);
 // Fresnel and semi-rough reflection retain white silhouettes while
 // the darker bevels create a readable sense of volume.
 vec3 fresnel=f0+(1.-f0)*pow(1.-nv,5.);
 vec3 color=albedo*(dark?.27:.34);
 color+=albedo*max(dot(n,normalize(vec3(.40,-.48,.56))),0.)*.13;
 color+=albedo*nl*.14;
 color+=env*fresnel*(dark?.64:.50);
 color+=direct*nl*vec3(1.03,1.07,1.13)*.40;
 float edge=pow(1.-nv,2.8);
 color+=vec3(.13,.27,.39)*edge*.25;
 // One near-invisible energy pulse on click, not a pulsing neon outline.
 color+=params.pulseValue*.09*vec3(.23,.55,.82);
 return color;
}
void main(){
 vec2 uv=qt_TexCoord0;
 vec2 screen=vec2((uv.x-.5)*2.33,
                  (.5-uv.y)*2.33*params.paneSize.y/max(1.,params.paneSize.x));
 screen/=clamp(params.viewZoom,.75,1.5);
 screen.y+=.06;
 bool dark=params.darkMode>.5;
 const float rad=.01745329252;
 vec2 rotation=vec2(
  -(params.orbitYaw+params.dragYaw)*rad+(params.pointer.x-.5)*.055,
  -params.dragPitch*rad+(params.pointer.y-.5)*.040);
 vec3 back=background(screen,uv,dark);
 vec3 origin=vec3(0.,0.,2.75);
 vec3 direction=normalize(vec3(screen,-2.75));
 float t=0.,nearest=10.,nearestT=0.,mat=0.;
 bool hit=false;
 for(int i=0;i<116;i++){
  vec3 p=origin+direction*t;
  vec2 shape=scene(p,rotation);
  if(shape.x<nearest){nearest=shape.x;nearestT=t;}
  if(shape.x<.00016){hit=true;mat=shape.y;break;}
  // A coarse lower bound skipped curved silhouettes at near-grazing
  // incidence. Smaller precision steps avoid staircase-like depth hits.
  t+=max(shape.x*.82,.00024);
  if(t>5.5)break;
 }
 vec3 color=back;
 if(hit) {
  color=materialAt(origin+direction*t,direction,rotation,mat,dark);
  // Screen-space edge antialiasing near silhouette, low impact on caps.
  float pixel=2.33/max(params.paneSize.x,1.);
  color=mix(color,back,smoothstep(pixel*.30,pixel*1.4,nearest)*.05);
 }else{
  float pixel=2.33/max(params.paneSize.x,1.);
  float coverage=1.-smoothstep(pixel*.25,pixel*1.45,nearest);
  if(coverage>.001){
   vec3 at=origin+direction*nearestT;
   color=mix(back,materialAt(at,direction,rotation,
                            scene(at,rotation).y,dark),coverage*.75);
  }
 }
 // Filmic, softly compressed highlights rather than flat Lambert shading.
 color=max(color,vec3(0.));
 color=(color*(2.51*color+.03))/(color*(2.43*color+.59)+.14);
 color=pow(clamp(color,vec3(0.),vec3(1.)),vec3(.94));
 fragColor=vec4(color,params.qt_Opacity);
}
