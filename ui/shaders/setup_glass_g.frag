#version 440
// The authoritative front silhouette is generated directly from
// ui/wasm/genycaster-icon.svg. R/G: wide signed distances (G/arrow).
// B: a narrow signed distance to their union, for clean surface normals.
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
vec3 canonicalDistance(vec2 xy) {
 // Source SVG viewBox 48 48 416 416 and central origin 256,256.
 vec2 svg=vec2(256.0+xy.x*220.0,256.0-xy.y*220.0);
 vec2 uv=(svg-vec2(48.0))/416.0;
 vec2 bounded=clamp(uv,vec2(.001),vec2(.999));
 vec3 rgb=texture(logoDistanceMap,bounded).rgb;
 vec2 wide=(rgb.rg*255.0-127.5)*(48.0/127.5)/220.0;
 float fine=(rgb.b*255.0-127.5)*(6.0/127.5)/220.0;
 if(uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0) {
  float outside=length((uv-bounded)*416.0/220.0);
  wide+=outside;
  fine+=outside;
 }
 return vec3(wide,fine);
}
vec2 logo(vec3 p) {
 vec3 contour=canonicalDistance(p.xy);
 // Identical extrusion for both exact path contours. Inset bevel
 // preserves the SVG outline on the front cap and at the arrow joint.
 const float halfDepth=.112;
 const float bevel=.007;
 float z=abs(p.z)-halfDepth;
 vec2 a=vec2(contour.x+bevel,z+bevel);
 vec2 b=vec2(contour.y+bevel,z+bevel);
 float dg=length(max(a,0.0))+min(max(a.x,a.y),0.0)-bevel;
 float da=length(max(b,0.0))+min(max(b.x,b.y),0.0)-bevel;
 float merged=min(dg,da);
 // Use a fine union field only close to the surface, where the
 // 8-bit wide SDF would otherwise create visible normal stripes.
 vec2 fineShape=vec2(contour.z+bevel,z+bevel);
 float fine=length(max(fineShape,0.0))
           +min(max(fineShape.x,fineShape.y),0.0)-bevel;
 float nearSurface=1.0-smoothstep(.014,.027,abs(merged));
 merged=mix(merged,fine,nearSurface);
 float arrow=1.0-smoothstep(-.006,.006,da-dg);
 return vec2(merged,arrow);
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
 // A quiet studio cyclorama: layered spotlights, subtle arc grid and
 // an edge vignette; never an image/DOM gimmick or a separate renderer.
 vec3 top=dark?vec3(.017,.029,.057):vec3(.914,.944,.974);
 vec3 bottom=dark?vec3(.046,.078,.119):vec3(.838,.885,.929);
 vec3 col=mix(top,bottom,smoothstep(0.,1.,uv.y));
 vec2 p=screen-vec2(-.04,.11);
 float spotlight=exp(-dot(p*vec2(1.15,.89),p*vec2(1.15,.89))*2.1);
 col+=(dark?vec3(.043,.086,.121):vec3(.065,.078,.083))*spotlight;
 float rim=length(screen*vec2(.83,1.18));
 float arc=exp(-pow((rim-.96)*170.,2.));
 col+=(dark?vec3(.050,.108,.161):vec3(.018,.045,.072))*arc*.25;
 col*=1.0-.105*smoothstep(.58,1.80,length(screen));
 float grain=hash21(floor(uv*params.paneSize))-.5;
 col+=grain*(dark?.0021:.0013);
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
 env+=vec3(.90,.94,1.0)*(1.55*stripA+.96*stripB+.55*softbox);
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
 vec3 silver=dark?vec3(.73,.80,.88):vec3(.69,.77,.83);
 vec3 titanium=dark?vec3(.31,.37,.45):vec3(.34,.40,.47);
 vec3 albedo=mix(silver,titanium,arrow);
 float roughness=mix(.23,.31,arrow);
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
 color+=env*fresnel*(dark?.71:.58);
 color+=direct*nl*vec3(1.03,1.07,1.13)*.53;
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
 vec3 back=background(screen,uv,dark);
 const float rad=.01745329252;
 vec2 rotation=vec2(
  -(params.orbitYaw+params.dragYaw)*rad+(params.pointer.x-.5)*.055,
  -params.dragPitch*rad+(params.pointer.y-.5)*.040);
 vec3 origin=vec3(0.,0.,2.75);
 vec3 direction=normalize(vec3(screen,-2.75));
 float t=0.,nearest=10.,nearestT=0.,mat=0.;
 bool hit=false;
 for(int i=0;i<92;i++){
  vec3 p=origin+direction*t;
  vec2 shape=scene(p,rotation);
  if(shape.x<nearest){nearest=shape.x;nearestT=t;}
  if(shape.x<.00085){hit=true;mat=shape.y;break;}
  t+=max(shape.x*.82,.0020);
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
