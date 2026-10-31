#version 440

// Glass G Arc: signed-distance extruded brand identity. Compiled by Qt Shader
// Baker to SPIR-V, OpenGL ES/WebGL, HLSL and Metal; no Qt Quick 3D dependency.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 paneSize;
    vec2 pointer;
    float darkMode;
    float pulseValue;
} params;

float sdBox(vec2 p, vec2 b) {
    vec2 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}

float sdTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0=p1-p0, e1=p2-p1, e2=p0-p2;
    vec2 v0=p-p0, v1=p-p1, v2=p-p2;
    vec2 pq0=v0-e0*clamp(dot(v0,e0)/dot(e0,e0),0.0,1.0);
    vec2 pq1=v1-e1*clamp(dot(v1,e1)/dot(e1,e1),0.0,1.0);
    vec2 pq2=v2-e2*clamp(dot(v2,e2)/dot(e2,e2),0.0,1.0);
    float s=sign(e0.x*e2.y-e0.y*e2.x);
    vec2 d=min(min(vec2(dot(pq0,pq0),s*(v0.x*e0.y-v0.y*e0.x)),
                   vec2(dot(pq1,pq1),s*(v1.x*e1.y-v1.y*e1.x))),
                   vec2(dot(pq2,pq2),s*(v2.x*e2.y-v2.y*e2.x)));
    return -sqrt(d.x)*sign(d.y);
}

vec2 brandDistance(vec3 p) {
    // Outer ring of the G, open in the upper-right quadrant. A distinct
    // bevelled forward triangle is lifted above the white ring.
    float ring = abs(length(p.xy + vec2(.035,.012)) - .50) - .118;
    float opening = sdBox(p.xy-vec2(.48,.38),vec2(.28,.31));
    ring=max(ring,-opening);
    float arrow = sdTriangle(p.xy,vec2(-.005,.128),vec2(.617,.128),vec2(.617,-.516));
    // Thin, real 3-D extrusions give the ring a bevelled silhouette and
    // catch independent specular highlights when the user moves the mouse.
    vec2 g=vec2(ring,abs(p.z)-.095);
    float dg=length(max(g,0.0))+min(max(g.x,g.y),0.0)-.012;
    vec2 a=vec2(arrow,abs(p.z-.12)-.09);
    float da=length(max(a,0.0))+min(max(a.x,a.y),0.0)-.007;
    return vec2(min(dg,da), da<dg ? 1.0 : 0.0);
}

vec3 rotateObject(vec3 p, vec2 angle) {
    float ay=angle.x, ax=angle.y;
    float c=cos(ay),s=sin(ay);
    p=vec3(c*p.x-s*p.z,p.y,s*p.x+c*p.z);
    c=cos(ax);s=sin(ax);
    return vec3(p.x,c*p.y+s*p.z,-s*p.y+c*p.z);
}

float sceneSdf(vec3 p, vec2 rotation) {
    return brandDistance(rotateObject(p,rotation)).x;
}

vec3 normalAt(vec3 p,vec2 rotation) {
    const float e=.0028;
    return normalize(vec3(
       sceneSdf(p+vec3(e,0.0,0.0),rotation)-sceneSdf(p-vec3(e,0.0,0.0),rotation),
       sceneSdf(p+vec3(0.0,e,0.0),rotation)-sceneSdf(p-vec3(0.0,e,0.0),rotation),
       sceneSdf(p+vec3(0.0,0.0,e),rotation)-sceneSdf(p-vec3(0.0,0.0,e),rotation)));
}

void main() {
    vec2 uv=qt_TexCoord0;
    bool dark=params.darkMode>.5;
    float ratio=params.paneSize.y/max(params.paneSize.x,1.0);
    vec2 screen=vec2((uv.x-.5)*2.33, (.5-uv.y)*2.33*ratio);
    screen.y+=.06;
    vec3 base=dark?vec3(.032,.068,.110):vec3(.932,.965,.989);
    vec3 bottom=dark?vec3(.055,.136,.204):vec3(.840,.914,.962);
    vec3 color=mix(base,bottom,smoothstep(0.0,1.0,uv.y));
    float glow=exp(-dot(screen,screen)*.68);
    color+=(dark?vec3(.018,.079,.100):vec3(.035,.018,.010))*glow;
    vec2 angle=(params.pointer-vec2(.5)) * vec2(.24,.13);
    vec3 ro=vec3(0.0,0.0,2.75);
    vec3 rd=normalize(vec3(screen,-2.75));
    float t=0.0;
    float material=0.0;
    bool hit=false;
    for(int i=0;i<52;i++) {
        vec3 p=ro+rd*t;
        vec2 d=brandDistance(rotateObject(p,angle));
        if(d.x<.0018) { hit=true; material=d.y; break; }
        t+=max(d.x*.84,.006);
        if(t>5.2) break;
    }
    if(hit) {
        vec3 position=ro+rd*t;
        vec3 normal=normalAt(position,angle);
        vec3 eye=normalize(ro-position);
        vec3 light=normalize(vec3(-.50,.75,.91));
        vec3 warmLight=normalize(vec3(.82,.18,.85));
        float ndl=max(dot(normal,light),0.0);
        float rim=pow(1.0-max(dot(normal,eye),0.0),2.8);
        float spec=pow(max(dot(reflect(-light,normal),eye),0.0),36.0);
        float warmSpec=pow(max(dot(reflect(-warmLight,normal),eye),0.0),65.0);
        vec3 glass=dark?vec3(.68,.86,.94):vec3(.39,.57,.70);
        vec3 arrow=dark?vec3(.12,.72,.86):vec3(.075,.48,.68);
        vec3 tint=mix(glass,arrow,material);
        float body=.35+.65*ndl;
        vec3 lit=tint*body;
        lit+=vec3(.70,.96,.99)*spec*.91;
        lit+=vec3(1.0,.72,.39)*warmSpec*.41;
        lit+=mix(vec3(.30,.65,.77),vec3(.15,.71,.83),material)*rim*.63;
        lit=mix(lit,vec3(1.0,.99,.96),clamp(spec*.28,0.0,.40));
        color=mix(color,lit, .97);
        color+=params.pulseValue*rim*vec3(.025,.11,.13);
    }
    fragColor=vec4(color,params.qt_Opacity);
}
