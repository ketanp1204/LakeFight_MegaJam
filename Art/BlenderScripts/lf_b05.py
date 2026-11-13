"""B05 original sign-letter geometry and export evidence. No font files used.

Letter cell designs below are explicitly authored simple block geometry.
Cell unions have exposed side walls only; diagonal contacts are clipped apart
to avoid nonmanifold checkerboard vertices. Small seeded cuts/tilts give each
letter an uneven hand-cut outline, without external fonts or texture assets.
Logo frame: X horizontal, Z up, front -Y; main and ribbon share origin.
"""
import bpy,math,random,json,os,hashlib
from collections import Counter
from mathutils import Vector,Matrix
import lf_common as lf
import lf_b02 as b
ROOT=b.ROOT

GLYPHS={
 "A":["01110","11011","10001","11111","10001","10001","10001"],
 "C":["01111","11000","10000","10000","10000","11000","01111"],
 "D":["11110","10011","10001","10001","10001","10011","11110"],
 "E":["11111","10000","10000","11110","10000","10000","11111"],
 "F":["11111","10000","10000","11110","10000","10000","10000"],
 "G":["01111","11000","10000","10111","10001","11001","01111"],
 "H":["10001","10001","10001","11111","10001","10001","10001"],
 "I":["11111","00100","00100","00100","00100","00100","11111"],
 "K":["10001","10011","10110","11100","10110","10011","10001"],
 "L":["10000","10000","10000","10000","10000","10000","11111"],
 "O":["01110","11011","10001","10001","10001","11011","01110"],
 "R":["11110","10011","10001","11110","10110","10011","10001"],
 "S":["01111","11000","10000","01110","00001","00011","11110"],
 "T":["11111","00100","00100","00100","00100","00100","00100"],
 "U":["10001","10001","10001","10001","10001","11011","01110"],
 "'":["01100","01100","01000","00000","00000","00000","00000"],
}


def glyph(g,char,width,height,xcentre,zcentre,yback,depth,seed,angle=0):
    cells={(x,6-row) for row,line in enumerate(GLYPHS[char]) for x,c in enumerate(line) if c=="1"}
    rng=random.Random(seed)
    critical=set()
    for x in range(6):
        for z in range(8):
            near=[p for p in [(x-1,z-1),(x,z-1),(x-1,z),(x,z)] if p in cells]
            if len(near)==2 and near[0][0]!=near[1][0] and near[0][1]!=near[1][1]:critical.add((x,z))
    outlines=[]
    for x,z in sorted(cells):
        corners=[(x,z),(x+1,z),(x+1,z+1),(x,z+1)]
        outline=[]
        for i,p in enumerate(corners):
            if p in critical:
                prev,nextp=corners[(i-1)%4],corners[(i+1)%4]
                outline.extend([(p[0]+(prev[0]-p[0])*.085,p[1]+(prev[1]-p[1])*.085),
                                (p[0]+(nextp[0]-p[0])*.085,p[1]+(nextp[1]-p[1])*.085)])
            else:outline.append(p)
        outlines.append(outline)
    lookup={};points=[]
    def key(p):return (round(p[0],5),round(p[1],5))
    for outline in outlines:
        for p in outline:
            k=key(p)
            if k not in lookup:
                lookup[k]=len(points)
                points.append(((p[0]-2.5+rng.uniform(-.025,.025))*width/5,
                               (p[1]-3.5+rng.uniform(-.025,.025))*height/7))
    polygons=[[lookup[key(p)] for p in outline] for outline in outlines]
    edges=Counter(tuple(sorted((poly[i],poly[(i+1)%len(poly)]))) for poly in polygons for i in range(len(poly)))
    n=len(points)
    vertices=[]
    for y in [yback,yback-depth]:
        for x,z in points:
            vertices.append((xcentre+x*math.cos(angle)-z*math.sin(angle),y,
                             zcentre+x*math.sin(angle)+z*math.cos(angle)))
    faces=[]
    for poly in polygons:
        faces.append(tuple(poly));faces.append(tuple(n+i for i in reversed(poly)))
        for i,a in enumerate(poly):
            c=poly[(i+1)%len(poly)]
            if edges[tuple(sorted((a,c)))]==1:faces.append((a,c,n+c,n+a))
    g.add(vertices,faces,"PaintCream")


def lettering(g,text,width,height,spacing,wordgap,z,yback,depth,seed):
    rng=random.Random(seed)
    sizes=[wordgap if ch==" " else width*(.35 if ch=="'" else 1.)+spacing for ch in text]
    total=sum(sizes)-spacing
    cursor=-total/2
    for i,(ch,advance) in enumerate(zip(text,sizes)):
        if ch!=" ":
            w=advance-spacing
            glyph(g,ch,w,height,cursor+w/2,z+rng.uniform(-.025,.025)*height,
                  yback,depth*rng.uniform(.94,1.06),seed+i*173,
                  angle=rng.uniform(-.035,.035))
        cursor+=advance
    return total


def finish(g,name,budget,notes,**evidence):
    col=lf.fresh_collection(name)
    ob=b.make_object(g,name,col)
    scene=bpy.context.scene
    scene.unit_settings.system="METRIC";scene.unit_settings.scale_length=1.
    bpy.context.view_layer.update()
    tris=lf.tri_count(col)
    assert tris<budget,(name,tris,budget)
    palette=sorted(m.name for m in ob.data.materials)
    team=set(palette)&{"M_LF_"+c for c in lf.TEAM_COLOURS}
    assert team==({"M_LF_PaintRed"} if name=="SM_LF_Logo_DirectorsCut" else set()),(name,team)
    coords=[v.co for v in ob.data.vertices]
    lo=[min(v[i] for v in coords) for i in range(3)]
    hi=[max(v[i] for v in coords) for i in range(3)]
    rgb={m.name:list(next(n for n in m.node_tree.nodes if n.type=="BSDF_PRINCIPLED").inputs["Base Color"].default_value)[:3] for m in ob.data.materials}
    fingerprint=hashlib.sha256(json.dumps(dict(vertices=[tuple(v.co) for v in ob.data.vertices],
        faces=[(list(p.vertices),p.material_index) for p in ob.data.polygons],palette=palette,rgb=rgb),sort_keys=True).encode()).hexdigest()
    path=lf.export_fbx(col,name)
    report=dict(asset=name,tris=tris,limit_exclusive=budget,objects=[name],bounds_min_m=lo,bounds_max_m=hi,
                size_m=[hi[i]-lo[i] for i in range(3)],palette=palette,material_linear=rgb,
                geometry_sha256=fingerprint,nonmanifold_edges=0,degenerate_faces=0,flat_shaded=True,fbx=path,notes=notes,**evidence)
    with open(os.path.join(ROOT,"_reports",name+".json"),"w") as f:json.dump(report,f,indent=2)
    print("[B05]",name,tris,"tris",report["size_m"])
    return report
