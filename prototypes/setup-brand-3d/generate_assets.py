#!/usr/bin/env python3
"""Create compact, deterministic glTF 2.0 GPU meshes; no external model assets."""
import json, math, struct
from pathlib import Path

HERE=Path(__file__).resolve().parent
ASSETS=HERE/"assets"
ASSETS.mkdir(exist_ok=True)
TAU=math.tau

def blade_shape(x0,y0,z0,height,width,phase,sweep,twist):
    positions=[]; normals=[]; indices=[]; edge_l=[]; edge_r=[]
    cols=12; rows=64
    for i in range(rows+1):
        u=i/rows
        xcenter=x0+70*math.sin((u-0.5)*1.48+phase)+sweep*(u-0.5)
        yy=y0+height*u
        midz=z0-33*math.sin(math.pi*u)+26*math.sin(2.3*u+phase)
        angle=twist+0.39*math.sin(math.pi*u+phase)
        w=width*(0.94+0.06*math.sin(math.pi*u))
        for j in range(cols+1):
            v=(j/cols-0.5)
            positions.append((xcenter+w*v*math.cos(angle), yy+17*v*math.sin(math.pi*u),
                              midz+w*v*math.sin(angle)+11*math.sin(math.pi*u)*math.sin(math.pi*j/cols)))
            nx=-math.sin(angle); ny=-0.14; nz=math.cos(angle)
            length=math.sqrt(nx*nx+ny*ny+nz*nz)
            normals.append((nx/length,ny/length,nz/length))
        edge_l.append(i*(cols+1))
        edge_r.append(i*(cols+1)+cols)
    for i in range(rows):
        for j in range(cols):
            a=i*(cols+1)+j;b=a+cols+1
            indices.extend((a,b,a+1, a+1,b,b+1))
    return {"pos":positions,"norm":normals,"indices":indices,
            "edge_l":edge_l,"edge_r":edge_r}

def wire_from_edge(shape,which,thickness=2.8):
    pts=[];norms=[];tris=[]
    edge=shape[which]
    for i,pidx in enumerate(edge):
        x,y,z=shape["pos"][pidx]
        pts.extend([(x-thickness,y,z+1.8),(x+thickness,y,z+1.8)])
        norms.extend([(0,0,1),(0,0,1)])
    for i in range(len(edge)-1):
        a=2*i;b=a+2
        tris.extend([a,b,a+1,a+1,b,b+1])
    return {"pos":pts,"norm":norms,"indices":tris}

def tube_arc(cx,cy,cz,radius,thickness,start,span,depth=1.0):
    positions=[];normals=[];indices=[]
    steps=128; sides=20
    for i in range(steps+1):
        t=i/steps
        angle=start+span*t
        taper=0.83+0.17*math.sin(math.pi*t)
        px=cx+radius*math.cos(angle);py=cy+radius*math.sin(angle)
        pz=cz+38*math.sin(1.7*angle)+25*math.sin(math.pi*t)
        for j in range(sides+1):
            phi=TAU*j/sides
            radial=math.cos(phi);deep=math.sin(phi)
            nx=radial*math.cos(angle);ny=radial*math.sin(angle);nz=deep
            positions.append((px+thickness*taper*nx,py+thickness*taper*ny,
                              pz+thickness*taper*depth*nz))
            normals.append((nx,ny,nz))
    for i in range(steps):
        for j in range(sides):
            a=i*(sides+1)+j;b=a+sides+1
            indices.extend((a,a+1,b, b,a+1,b+1))
    return {"pos":positions,"norm":normals,"indices":indices}

def sphere(x,y,z,r):
    positions=[];normals=[];indices=[]
    for i in range(25):
        phi=math.pi*i/24
        for j in range(33):
            theta=TAU*j/32
            nx=math.sin(phi)*math.cos(theta);ny=math.cos(phi);nz=math.sin(phi)*math.sin(theta)
            normals.append((nx,ny,nz))
            positions.append((x+r*nx,y+r*ny,z+r*nz))
    for i in range(24):
        for j in range(32):
            a=i*33+j;b=a+33
            indices.extend((a,a+1,b,b,a+1,b+1))
    return {"pos":positions,"norm":normals,"indices":indices}

def make_glb(dark):
    scheme=(
        [("#177c96",.33,.19,.16),("#0b90a7",.30,.21,.17),("#2d84ba",.31,.25,.18),
         ("#f5c98b",.99,.20,.12),("#43e6dd",.93,.18,.09),("#a5fbeb",.93,.15,.07)]
        if dark else
        [("#7ac9d6",.28,.14,.13),("#41a9cb",.28,.16,.14),("#b5e6ee",.27,.12,.14),
         ("#f2c681",.90,.10,.13),("#2ac7d3",.89,.13,.08),("#80dfef",.85,.09,.09)]
    )
    def mat(spec,index):
        hexcol,alpha,metal,rough=spec
        rgb=[int(hexcol[j:j+2],16)/255. for j in (1,3,5)]
        return {"name":f"GenyCaster_material_{index}", "pbrMetallicRoughness":{
            "baseColorFactor":[*rgb,alpha],"metallicFactor":metal,"roughnessFactor":rough},
            "alphaMode":"BLEND" if alpha<1 else "OPAQUE","doubleSided":True,
            "emissiveFactor":[rgb[k]*(0.32 if index>=4 else 0.10) for k in range(3)]}
    materials=[mat(spec,i) for i,spec in enumerate(scheme)]
    objects=[
        ("GlassBlade_A",blade_shape(-110,-298,-65,570,164,-.5,-52,.34),0),
        ("GlassBlade_B",blade_shape(38,-242,57,493,175,.65,60,-.25),1),
        ("GlassBlade_C",blade_shape(138,-298,-126,485,130,1.6,34,.28),2),
    ]
    # Thin metallic emissive edges trace the contours of the same surfaces.
    for idx,(name,geo,matidx) in enumerate(objects[:3]):
        objects.append((name+"_rim",wire_from_edge(geo,"edge_r",1.35),3 if idx==1 else 4))
    objects.append(("GlassHelixPrimary",tube_arc(-50,22,194,153,25,2.64,4.52,0.83),4))
    objects.append(("GlassHelixSecondary",tube_arc(-106,-13,222,100,15,-1.76,3.9,0.85),5))
    objects.append(("FloatingPearl",sphere(144,196,250,26),5))
    binary=bytearray();bufferViews=[];accessors=[];meshes=[];nodes=[]
    def blob(data,fmt,ctype,target,atype,minmax=False):
        offset=len(binary)
        while len(binary)%4:binary.append(0)
        offset=len(binary)
        binary.extend(struct.pack("<"+fmt*len(data),*data))
        view={"buffer":0,"byteOffset":offset,"byteLength":len(data)*struct.calcsize("<"+fmt),"target":target}
        bufferViews.append(view)
        accessor={"bufferView":len(bufferViews)-1,"componentType":ctype,"count":len(data)//(3 if atype=="VEC3" else 1),"type":atype}
        if minmax and atype=="VEC3":
            xyz=[data[i::3] for i in range(3)]
            accessor["min"]=[min(v) for v in xyz];accessor["max"]=[max(v) for v in xyz]
        accessors.append(accessor)
        return len(accessors)-1
    for name,obj,midx in objects:
        pos=[v for p in obj["pos"] for v in p]
        norm=[v for n in obj["norm"] for v in n]
        ind=obj["indices"]
        pa=blob(pos,"f",5126,34962,"VEC3",True)
        na=blob(norm,"f",5126,34962,"VEC3")
        ia=blob(ind,"I",5125,34963,"SCALAR")
        meshes.append({"name":name,"primitives":[{"attributes":{"POSITION":pa,"NORMAL":na},
                 "indices":ia,"material":midx,"mode":4}]})
        nodes.append({"name":name,"mesh":len(meshes)-1})
    gltf={"asset":{"version":"2.0","generator":"GenyCaster deterministic procedural brand scene"},
          "scene":0,"scenes":[{"nodes":list(range(len(nodes)))}],
          "nodes":nodes,"meshes":meshes,"materials":materials,
          "buffers":[{"byteLength":len(binary)}],"bufferViews":bufferViews,"accessors":accessors}
    jsonchunk=json.dumps(gltf,separators=(",",":"),ensure_ascii=False).encode()
    while len(jsonchunk)%4:jsonchunk+=b" "
    while len(binary)%4:binary.append(0)
    total=12+8+len(jsonchunk)+8+len(binary)
    data=struct.pack("<4sII",b"glTF",2,total)
    data+=struct.pack("<I4s",len(jsonchunk),b"JSON")+jsonchunk
    data+=struct.pack("<I4s",len(binary),b"BIN\\x00".decode("unicode_escape").encode("latin1"))+binary
    path=ASSETS/("brand-dark.glb" if dark else "brand-light.glb")
    path.write_bytes(data)
    print(path.name,path.stat().st_size,"bytes",len(objects),"objects")

if __name__=="__main__":
    make_glb(True);make_glb(False)
