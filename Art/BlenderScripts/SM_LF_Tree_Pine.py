"""B03 chunky asymmetric pine, original solid palette mesh; base origin."""
import sys, importlib, math, random
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
g=b3.b.Geometry()
g.cylinder((0,0,0),(.06,.03,3.45),.22,"WoodDark",sides=7,end_radius=.11)
for i in range(7):
    x,y,z=g.vertices[i]
    g.vertices[i]=(x,y,0.)
rng=random.Random(20261113034)
for layer,(z,r,h) in enumerate([(1.35,1.40,1.80),(2.32,1.12,1.65),(3.28,.79,1.78)]):
    sides=7
    rings=[]
    for dz,scale in [(0,1.),(.42,.86),(h,.035)]:
        rings.append([(.07*layer+r*scale*math.cos(j*math.tau/sides+layer*.4),
                       -.03*layer+r*scale*math.sin(j*math.tau/sides+layer*.4),
                       z+dz+(rng.uniform(-.12,.12) if dz==0 else 0)) for j in range(sides)])
    g.loft(rings,"Moss" if layer%2==0 else "Reed")
REPORTS=[b3.finish(g,"SM_LF_Tree_Pine",600,notes="Three chunky staggered seven-sided foliage skirts and visible dark trunk; 5.06m tall.")]
