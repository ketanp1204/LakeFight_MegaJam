"""B03 half-submerged broken hollow stump, waterline origin."""
import sys, importlib, math
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
g=b3.b.Geometry()
n=9
zs=[.60,.57,.66,.59,.53,.65,.62,.54,.63]
rings=[]
for k in range(4):
    ring=[]
    for j in range(n):
        a=j*math.tau/n
        r=[.46,.34,.235,.20][k]*(1+.08*math.sin(j*3))
        z=-.65 if k==0 else zs[j] if k in [1,2] else .30
        ring.append((r*math.cos(a),r*math.sin(a),z))
    rings.append(ring)
g.loft(rings, ["WoodDark" if j%3==0 else "Wood" for j in range(n)]+["Rope"]*n+["WoodDark"]*n+["WoodDark","WoodDark"])
for j in range(6):
    a=j*math.tau/6+.2
    g.cylinder((.24*math.cos(a),.24*math.sin(a),-.30),
               (.85*math.cos(a),.85*math.sin(a),-.67),.18,"WoodDark",sides=5,end_radius=.06)
g.cylinder((.22,.05,.15),(.68,.18,.46),.15,"Wood",sides=6,end_radius=.09)
REPORTS=[b3.finish(g,"SM_LF_Stump",origin="waterline centre Z=0",notes="Broken hollow cut, pale end-grain rim, six submerged buttress roots and one snapped branch. Main trunk -0.65..+0.66m.")]
