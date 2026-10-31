#!/usr/bin/env python3
"""Deterministic curved *solid* glass panels / ribbons with proper surface normals.

All meshes are self-contained glTF 2.0; no downloads, Blender, or tracked build
products are needed. The bevelled skins are given physically useful tangent
normals instead of the previous flat, constant normals.
"""
from pathlib import Path
import json, math, struct
TAU=2*math.pi
ROOT=Path(__file__).resolve().parent
ASSETS=ROOT/"assets"
ASSETS.mkdir(parents=True,exist_ok=True)

def normalize(v):
    mag=math.sqrt(sum(x*x for x in v)) or 1
    return tuple(x/mag for x in v)

def cross(a,b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

def sub(a,b):return tuple(x-y for x,y in zip(a,b))

def panel(cx,cy,cz,radius,width,start,span,twist):
    """Curved glass architectural blade: a thick annular sweep, NOT a flat slab.

    Each band bends in X/Y and changes depth in Z. The outer rim follows the
    same surface as the glass, so it cannot float away when the camera moves.
    """
    rows=110;cols=26
    pos=[];norm=[];indices=[]
    def surface(u,v,depth):
        theta=start+span*u+0.025*math.sin(math.pi*v)
        rad=radius+(v-.5)*width
        xx=cx+rad*math.cos(theta)
        yy=cy+rad*math.sin(theta)
        zz=cz+33*math.sin(theta*1.45)+19*math.sin(math.pi*v)
        zz+=twist*(u-.5)*(v-.5)*width
        return (xx,yy,zz+depth)
    for layer in [-1,1]:
        for i in range(rows+1):
            u=i/rows
            for j in range(cols+1):
                v=j/cols
                p=surface(u,v,layer*5.2)
                pu=surface(u+.001,v,layer*5.2) if u<.999 else surface(u-.001,v,layer*5.2)
                pv=surface(u,v+.001,layer*5.2) if v<.999 else surface(u,v-.001,layer*5.2)
                n=normalize(cross(sub(pv,p),sub(pu,p)))
                if layer==-1:n=tuple(-x for x in n)
                pos.append(p);norm.append(n)
    layer_size=(rows+1)*(cols+1)
    for layer in (1,):
        # Show the front polished surface; keep rear vertices for glTF
        # volume bookkeeping but do not double-blend coincident glass
        # surfaces. Rim strips supply the physical thickness visually.
        off=layer*layer_size
        for i in range(rows):
            for j in range(cols):
                a=off+i*(cols+1)+j;b=a+cols+1
                if layer==1:indices.extend((a,a+1,b,b,a+1,b+1))
                else:indices.extend((a,b,a+1,b,b+1,a+1))
    perimeter=[]
    perimeter.extend([j for j in range(cols+1)])
    perimeter.extend([i*(cols+1)+cols for i in range(1,rows+1)])
    perimeter.extend([rows*(cols+1)+j for j in range(cols-1,-1,-1)])
    perimeter.extend([i*(cols+1) for i in range(rows-1,0,-1)])
    for i in range(len(perimeter)):
        a=perimeter[i];b=perimeter[(i+1)%len(perimeter)]
        c=a+layer_size;d=b+layer_size
        indices.extend((a,b,c,c,b,d))
    return dict(pos=pos,norm=norm,indices=indices,
                right=[surface(i/rows,1,6) for i in range(rows+1)],
                top=[surface(1,j/cols,6) for j in range(cols+1)])

def tube_curve(points,radius=1.8,sides=10):
    p=[];n=[];tri=[]
    for i,c in enumerate(points):
        axis=normalize(sub(points[min(i+1,len(points)-1)],points[max(i-1,0)]))
        ref=(0,0,1)
        t=normalize(cross(axis,ref))
        if sum(v*v for v in t)<.1:t=(1,0,0)
        b=normalize(cross(axis,t))
        for j in range(sides+1):
            theta=TAU*j/sides
            off=tuple(radius*(math.cos(theta)*t[k]+math.sin(theta)*b[k]) for k in range(3))
            p.append(tuple(c[k]+off[k] for k in range(3)))
            n.append(normalize(off))
    for i in range(len(points)-1):
        for j in range(sides):
            a=i*(sides+1)+j;b=a+sides+1
            tri.extend((a,b,a+1,a+1,b,b+1))
    return dict(pos=p,norm=n,indices=tri)

def ribbon(cx,cy,cz,radius,thickness,start,span):
    """Sculptural sweeping slender 3-D glass arc, with tangent-correct normals."""
    points=[]
    steps=96
    for i in range(steps+1):
        u=i/steps
        a=start+span*u
        points.append((cx+radius*math.cos(a), cy+radius*math.sin(a)*.69,
                       cz+22*math.sin(2*a+.35)+19*(u-.5)))
    return tube_curve(points,thickness,14)

def orb(cx,cy,cz,r):
    p=[];n=[];tri=[]
    rows=28;cols=40
    for i in range(rows+1):
        phi=math.pi*i/rows
        for j in range(cols+1):
            a=TAU*j/cols
            normal=(math.sin(phi)*math.cos(a),math.cos(phi),math.sin(phi)*math.sin(a))
            p.append((cx+r*normal[0],cy+r*normal[1],cz+r*normal[2]))
            n.append(normal)
    for i in range(rows):
        for j in range(cols):
            a=i*(cols+1)+j;b=a+cols+1
            tri.extend((a,b,a+1,a+1,b,b+1))
    return dict(pos=p,norm=n,indices=tri)

def render_glb(dark):
    scheme=(
        [
            ("#43BED2",.48,.03,.07),("#82D6D5",.46,.03,.09),("#84A6DF",.43,.04,.09),
            ("#EBD5A1",.92,.10,.10),("#68E8D8",.96,.13,.08),("#B5FFF1",.94,.05,.08),
            ("#A8EAE6",.78,.07,.10),
        ] if dark else [
            ("#6AC5D3",.41,.03,.06),("#6FBCD8",.39,.02,.08),("#94CADD",.37,.03,.08),
            ("#E7C78A",.84,.10,.10),("#13B5CA",.91,.13,.08),("#D6FEFA",.82,.04,.09),
            ("#9BE9E9",.70,.07,.10)
        ]
    )
    materials=[]
    for idx,(hexrgb,alpha,metal,rough) in enumerate(scheme):
        rgb=[int(hexrgb[i:i+2],16)/255 for i in (1,3,5)]
        materials.append({
            "name":["FrostedGlass_01","FrostedGlass_02","FrostedGlass_03",
                    "GoldEdge","TurquoiseEdge","GlassPearl","SculpturalGlass"][idx],
            "doubleSided":True,
            "alphaMode":"BLEND" if alpha<1 else "OPAQUE",
            "pbrMetallicRoughness":{
                "baseColorFactor":[*rgb,alpha],"roughnessFactor":rough,
                "metallicFactor":metal
            },
            "emissiveFactor":[v*(.26 if idx in [3,4] else .12) for v in rgb],
            "extensions":{"KHR_materials_ior":{"ior":1.45}}
        })
    panels=[
      ("GlassPanel_Front",panel(-345,-62,100,445,114,-.61,1.77,.14),0),
      ("GlassPanel_Middle",panel(-334,-68,-70,515,112,-.61,1.78,-.12),1),
      ("GlassPanel_Back",panel(-306,-48,-145,367,103,-.62,1.75,.18),2)
    ]
    parts=panels[:]
    for i,(_,geometry,_) in enumerate(panels):
        edge=geometry["right"]+geometry["top"]
        parts.append((f"ReflectiveEdge_{i}",tube_curve(geometry["right"],2.7 if i==0 else 1.9),3 if i==1 else 4))
        parts.append((f"UpperBevel_{i}",tube_curve(geometry["top"],1.5),3 if i==1 else 4))
    parts.append(("SculpturalRibbon",ribbon(-110,-118,45,118,9,2.1,2.8),6))
    parts.append(("FloatingGlassOrb",orb(144,197,210,27),5))
    data=bytearray();views=[];accessors=[];meshes=[];nodes=[]
    def add(data_flat,fmt,ctype,target,kind,minmax=False):
        while len(data)%4:data.append(0)
        ofs=len(data)
        # safe local buffer rather than shadowing outer data
        return ofs
    def blob(values,fmt,ctype,target,atype,bounds=False):
        while len(data)%4:data.append(0)
        offset=len(data)
        data.extend(struct.pack("<"+fmt*len(values),*values))
        views.append({"buffer":0,"byteOffset":offset,"byteLength":len(data)-offset,"target":target})
        size=3 if atype=="VEC3" else 1
        accessor={"bufferView":len(views)-1,"componentType":ctype,"count":len(values)//size,"type":atype}
        if bounds:
            v=[values[j::3] for j in range(3)]
            accessor.update(min=[min(x) for x in v],max=[max(x) for x in v])
        accessors.append(accessor)
        return len(accessors)-1
    for name,geom,mi in parts:
        p=blob([v for xyz in geom["pos"] for v in xyz],"f",5126,34962,"VEC3",True)
        n=blob([v for xyz in geom["norm"] for v in xyz],"f",5126,34962,"VEC3")
        ix=blob(geom["indices"],"I",5125,34963,"SCALAR")
        meshes.append({"name":name,"primitives":[{
            "attributes":{"POSITION":p,"NORMAL":n},"indices":ix,"material":mi,"mode":4
        }]})
        nodes.append({"name":name,"mesh":len(meshes)-1})
    manifest={
        "asset":{"version":"2.0","generator":"GenyCaster GPU-Glass v2"},
        "extensionsUsed":["KHR_materials_ior"],
        "scene":0,"scenes":[{"nodes":list(range(len(nodes)))}],
        "meshes":meshes,"nodes":nodes,"materials":materials,
        "buffers":[{"byteLength":len(data)}],"bufferViews":views,"accessors":accessors
    }
    js=json.dumps(manifest,ensure_ascii=False,separators=(",",":")).encode()
    while len(js)%4:js+=b" "
    while len(data)%4:data.append(0)
    content=struct.pack("<4sII",b"glTF",2,12+8+len(js)+8+len(data))
    content+=struct.pack("<I4s",len(js),b"JSON")+js
    content+=struct.pack("<I4s",len(data),b"BIN\x00")+data
    f=ASSETS/("brand-dark.glb" if dark else "brand-light.glb")
    f.write_bytes(content)
    print(f.name,len(content),"bytes",len(parts),"meshes")

if __name__=="__main__":
    render_glb(True)
    render_glb(False)
