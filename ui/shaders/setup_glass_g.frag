#version 440
// Render the ORIGINAL icon's path geometry, not reconstructed circles or
// triangles. The precomputed distance atlas is generated without manual
// geometry from ui/wasm/genycaster-icon.svg at 2 SVG pixels per atlas pixel.
// The original G/arrow silhouettes remain exact at front view; depth is an
// extrusion in world Z with narrow bevels applied *inside* the contours.
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
} params;
layout(binding=1) uniform sampler2D logoDistanceMap;

vec3 objectSpace(vec3 p,vec2 rot) {
 float c=cos(rot.x),s=sin(rot.x);
 p=vec3(c*p.x-s*p.z,p.y,s*p.x+c*p.z);
 c=cos(rot.y);s=sin(rot.y);
 return vec3(p.x,c*p.y+s*p.z,-s*p.y+c*p.z);
}

vec2 canonicalDistance(vec2 xy) {
 // SVG viewBox 48 48 416 416, with SVG center (256,256).
 // Object unit maps to 220 SVG units. Images are top-left origin, as is SVG.
 vec2 svg=vec2(256.0+xy.x*220.0,256.0-xy.y*220.0);
 vec2 uv=(svg-vec2(48.0))/416.0;
 vec2 bounded=clamp(uv,vec2(.001),vec2(.999));
 vec2 rg=texture(logoDistanceMap,bounded).rg;
 // PNG channels store signed distance in original SVG units, centered at 127.5.
 vec2 d=(rg*255.0-127.5)*(48.0/127.5)/220.0;
 vec2 beyond=(uv-bounded)*416.0/220.0;
 // Keep a conservative positive distance outside the source SVG viewBox.
 if (uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0)
     d+=length(beyond);
 return d;
}

vec2 logo(vec3 p) {
 vec2 contour=canonicalDistance(p.xy);
 // Shared extrusion and inset corner radius. Inset-by-bevel is essential:
 // at the front face the SDF is zero at the original SVG path, not outside it.
 const float halfDepth=.112;
 const float bevel=.007;
 vec2 z=vec2(abs(p.z)-halfDepth);
 vec2 a=vec2(contour.x+bevel,z.x+bevel);
 vec2 b=vec2(contour.y+bevel,z.x+bevel);
 float dg=length(max(a,0.0))+min(max(a.x,a.y),0.0)-bevel;
 float da=length(max(b,0.0))+min(max(b.x,b.y),0.0)-bevel;
 // No geometry-changing 2D smooth-union: the official glyph outline is exact.
 float merged=min(dg,da);
 float arrow=1.0-smoothstep(-.006,.006,da-dg);
 return vec2(merged,arrow);
}
vec2 scene(vec3 p,vec2 angles){return logo(objectSpace(p,angles));}
vec3 normalAt(vec3 p,vec2 angles){
 const float h=.0035;
 vec3 a=vec3(1.,-1.,-1.),b=vec3(-1.,-1.,1.);
 vec3 c=vec3(-1.,1.,-1.),d=vec3(1.,1.,1.);
 return normalize(a*scene(p+h*a,angles).x+
                  b*scene(p+h*b,angles).x+
                  c*scene(p+h*c,angles).x+
                  d*scene(p+h*d,angles).x);
}
vec3 background(vec2 screen,vec2 uv,bool dark){
 vec3 top=dark?vec3(.031,.071,.112):vec3(.942,.967,.990);
 vec3 bottom=dark?vec3(.055,.135,.200):vec3(.850,.916,.965);
 vec3 tone=mix(top,bottom,smoothstep(0.,1.,uv.y));
 return tone+(dark?vec3(.024,.065,.079):vec3(.018,.021,.024))*exp(-dot(screen,screen));
}
vec3 materialAt(vec3 p,vec3 ray,vec2 rotation,float arrow,bool dark){
 vec3 n=normalAt(p,rotation);
 vec3 eye=normalize(-ray);
 vec3 key=normalize(vec3(-.55,.81,.85));
 vec3 warm=normalize(vec3(.82,.17,.78));
 float kd=max(dot(n,key),0.0);
 float fill=max(dot(n,vec3(.25,-.52,.81)),0.0);
 float edge=pow(1.0-max(dot(n,eye),0.0),3.3);
 float shine=pow(max(dot(reflect(-key,n),eye),0.0),72.0);
 float champagne=pow(max(dot(reflect(-warm,n),eye),0.0),96.0);
 // Official artwork: white G, neutral gray directional mark. No cyan arrow.
 vec3 silver=dark?vec3(.89,.94,.98):vec3(.57,.66,.73);
 vec3 arrowGray=dark?vec3(.64,.68,.73):vec3(.42,.49,.55);
 vec3 tint=mix(silver,arrowGray,arrow);
 vec3 outColor=tint*(.58+.45*kd+.19*fill);
 outColor+=vec3(.92,.97,1.)*shine*.68;
 outColor+=vec3(1.,.83,.66)*champagne*.16;
 outColor+=vec3(.30,.60,.73)*edge*.25;
 return outColor;
}
void main(){
 vec2 uv=qt_TexCoord0;
 vec2 screen=vec2((uv.x-.5)*2.33,
                  (.5-uv.y)*2.33*params.paneSize.y/max(1.,params.paneSize.x));
 screen.y+=.06;
 bool dark=params.darkMode>.5;
 vec3 back=background(screen,uv,dark);
 const float rad=.01745329252;
 vec2 rotation=vec2(
  -(params.orbitYaw+params.dragYaw)*rad+(params.pointer.x-.5)*.084,
  -params.dragPitch*rad+(params.pointer.y-.5)*.055);
 vec3 origin=vec3(0.,0.,2.75);
 vec3 direction=normalize(vec3(screen,-2.75));
 float t=0.0;
 float nearest=10.,nearestT=0.;
 float mat=0.;
 bool hit=false;
 for(int i=0;i<76;i++){
  vec3 p=origin+direction*t;
  vec2 shape=scene(p,rotation);
  if(shape.x<nearest){nearest=shape.x;nearestT=t;}
  if(shape.x<.0013){hit=true;mat=shape.y;break;}
  t+=max(shape.x*.76,.0038);
  if(t>5.5)break;
 }
 vec3 color=back;
 if(hit) {
  color=materialAt(origin+direction*t,direction,rotation,mat,dark);
  color+=params.pulseValue*.03*vec3(.65,.83,.91);
  float pixel=2.33/max(params.paneSize.x,1.);
  color=mix(color,back,smoothstep(pixel*.33,pixel*1.5,nearest)*.12);
 } else {
  float pixel=2.33/max(params.paneSize.x,1.);
  float cover=1.-smoothstep(pixel*.15,pixel*1.45,nearest);
  if(cover>.001){
   vec3 nearPos=origin+direction*nearestT;
   float id=scene(nearPos,rotation).y;
   color=mix(back,materialAt(nearPos,direction,rotation,id,dark),cover*.74);
  }
 }
 color=max(color,vec3(0.));
 color=color/(vec3(1.)+color*.18);
 color=pow(color,vec3(.98));
 fragColor=vec4(color,params.qt_Opacity);
}
