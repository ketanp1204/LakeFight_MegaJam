"""B03 chunky broadleaf shore tree, five original faceted foliage masses."""
import sys, importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
g=b3.b.Geometry()
g.cylinder((0,0,0),(.14,0,2.9),.29,"WoodDark",sides=7,end_radius=.16)
for i in range(7):
    x,y,z=g.vertices[i]
    g.vertices[i]=(x,y,0.)
for end in [(-.85,-.3,3.2),(1.,.1,3.5),(.1,.8,3.6)]:
    g.cylinder((.06,0,1.7),end,.14,"Wood",sides=5,end_radius=.075)
for idx,(c,r,h) in enumerate([((-.90,-.30,3.25),1.05,1.9),((.90,-.15,3.55),1.2,2.),
                             ((.1,.85,3.72),1.15,1.95),((0,-.65,4.02),1.,1.85),((.05,.10,4.6),1.02,1.6)]):
    b3.crown(g,c,r,h,20261113040+idx,sides=7)
REPORTS=[b3.finish(g,"SM_LF_Tree_Round",600,notes="Forked chunky trunk with five irregular solid faceted crowns; base at Z=0.")]
