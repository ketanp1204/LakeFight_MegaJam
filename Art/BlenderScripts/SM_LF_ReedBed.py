"""B03 ReedBed v2: Poisson scattered upright narrow blades and thirty cattails.

All leaves are closed 10mm shells; deterministic seed, never a row/column grid.
The B02.2 wide-leaf screen is retained separately in SM_LF_ReedBed_v1.py.
"""
import sys, importlib, random, math, json, os
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import bpy

g=b3.b.Geometry()
rng=random.Random(202611130302)
points=[]
for attempt in range(10000):
    p=Vector((rng.uniform(-1.65,1.65),rng.uniform(-1.12,1.12),.004))
    if all((p-q).length>.09 for q in points):
        points.append(p)
    if len(points)==270: break
assert len(points)==270
thicknesses=[]
heights=[]
for i,p in enumerate(points):
    a=rng.uniform(0,math.tau)
    direction=Vector((math.cos(a),math.sin(a),0))
    across=Vector((-direction.y,direction.x,0))
    h=1.796 if i==0 else rng.uniform(1.196,1.785)
    heights.append(h+.004)
    lean=rng.uniform(.12,.32)
    tip=p+direction*lean+Vector((0,0,h))
    width=rng.uniform(.09,.135)
    # A narrow rooted spear uses eight tris instead of a twelve-tri diamond.
    # Spend the saving on 90 additional blades: dense at the waterline, finer above.
    thicknesses.extend(b3.shell(g,[p-across*width/2,p+across*width/2,tip]))
for i in range(36):
    p=points[(i*37)%270].copy()
    h=rng.uniform(1.25,1.79)
    head=i<30
    top=p+Vector((rng.uniform(-.04,.04),rng.uniform(-.04,.04),h-(.17 if head else 0)))
    g.cylinder(p,top,.009,"Reed",sides=4,end_radius=.005)
    if head:
        g.loft([[(top.x+r*math.cos(j*math.tau/5),top.y+r*math.sin(j*math.tau/5),top.z+z)
                 for j in range(5)] for z,r in [(0,.012),(.125,.026),(.17,.010)]],"WoodDark")
for i in range(8):
    p=points[(i*23+7)%270]
    a=rng.uniform(0,math.tau)
    direction=Vector((math.cos(a),math.sin(a),0))
    across=Vector((-direction.y,direction.x,0))
    tip=p+direction*rng.uniform(.50,.68)+Vector((0,0,rng.uniform(.6,.9)))
    shoulder=p.lerp(tip,.30)
    width=rng.uniform(.42,.58)
    thicknesses.extend(b3.shell(g,[p,shoulder-across*width/2,tip,shoulder+across*width/2],"Moss"))
g.fit()
report=b3.finish(g,"SM_LF_ReedBed",3500,
    notes="v2: 270 Poisson-distributed narrow upright 1.2-1.8m blades, eight broad low leaves, 36 stems and thirty cattails. Closed true-10mm leaf shells. No rows, no external mesh, seed 202611130302.",
    version=2,thin_blade_count=270,broad_leaf_count=8,stem_count=36,cattail_count=30,
    placement="seeded Poisson rejection; min root separation 0.09m",seed=202611130302,
    thin_blade_height_range_m=[min(heights),max(heights)],
    leaf_thickness_range_m=[min(thicknesses),max(thicknesses)],
    root_positions_before_centring_m=[list(p) for p in points])
tree=BVHTree.FromObject(bpy.data.objects["SM_LF_ReedBed"],bpy.context.evaluated_depsgraph_get())
probes={}
for sign in [-1,1]:
    hits=0
    for j in range(121):
        for k in range(19):
            start=Vector((-1.5+j*3/120,sign*2.,.10+k*.45/18))
            hits+=tree.ray_cast(start,Vector((0,-sign,0)),4.)[0] is not None
    probes[str(sign)+"Y"]=dict(hits=hits,samples=121*19,fraction=round(hits/(121*19),5))
report["side_screen_probe"]=dict(window="3m wide x Z=.10.. .55m",directions=probes)
with open(os.path.join(b3.b.ROOT,"_reports","SM_LF_ReedBed.json"),"w") as f: json.dump(report,f,indent=2)
print("[B03 REED SCREEN]",report["side_screen_probe"])
REPORTS=[report]
