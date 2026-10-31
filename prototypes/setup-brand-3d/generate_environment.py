#!/usr/bin/env python3
"""Generate portable equirectangular studio light probes from simple softboxes."""
from pathlib import Path
import math,struct,zlib
OUT=Path(__file__).resolve().parent/"assets"
OUT.mkdir(exist_ok=True)
W,H=640,320

def gauss(x,c,s):
    return math.exp(-.5*((x-c)/s)**2)

def write_png(path,pixels):
    def chunk(tag,payload):
        return struct.pack(">I",len(payload))+tag+payload+struct.pack(">I",zlib.crc32(tag+payload)&0xffffffff)
    raw=b"".join(b"\x00"+row for row in pixels)
    data=(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",W,H,8,2,0,0,0))
          +chunk(b"IDAT",zlib.compress(raw,6))+chunk(b"IEND",b""))
    path.write_bytes(data)
    print(path.name,len(data))
for dark in (True,False):
    rows=[]
    for yy in range(H):
        y=yy/(H-1)
        pix=bytearray()
        for xx in range(W):
            x=xx/(W-1)
            env=(.035+.16*gauss(y,.22,.33)) if dark else .39+.21*gauss(y,.3,.36)
            r=env*(.50 if dark else .75)
            g=env*(.84 if dark else .88)
            b=env*(1.12 if dark else 1.02)
            # Broad white commercial studio softboxes, plus a cold cyan strip
            # and a slim champagne-gold side light.
            boxes=[
                (.14,.43,.075,.31,(.43,.91,1.0),.91),
                (.42,.37,.13,.42,(.79,.98,1.00),.52),
                (.71,.39,.035,.28,(1.,.72,.39),.98),
                (.85,.47,.095,.35,(.39,.79,1.),.68),
                (.50,.06,.41,.095,(.92,1.00,1.00),.48)
            ]
            for cx,cy,sx,sy,col,amp in boxes:
                energy=gauss(x,cx,sx)*gauss(y,cy,sy)*amp
                r+=col[0]*energy;g+=col[1]*energy;b+=col[2]*energy
            pix.extend(max(0,min(255,int(v*255))) for v in (r,g,b))
        rows.append(bytes(pix))
    write_png(OUT/("studio-dark.png" if dark else "studio-light.png"),rows)
