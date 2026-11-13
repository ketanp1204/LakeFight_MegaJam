"""B03 eight solid flat notched pads and two five-petal flowers; <300 tris."""
import sys, importlib, math, random
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_b03 as b3
importlib.reload(b3)
from mathutils import Vector
g=b3.b.Geometry()
rng=random.Random(20261113033)
pads=[(-.83,-.34,.34),(-.22,-.55,.29),(.43,-.49,.36),(.92,.05,.30),
      (.38,.28,.38),(-.25,.13,.30),(-.83,.40,.31),(-.12,.72,.26)]
for index,(x,y,r) in enumerate(pads):
    angle=rng.random()*math.tau
    # Seven points on the circumference plus the V-notch point: 28 tris/pad.
    angles=[.25+j*(math.tau-.5)/6+angle for j in range(7)]
    outline=[(x+r*math.cos(a), y+r*math.sin(a)) for a in angles]
    outline.append((x+.10*r*math.cos(angle), y+.10*r*math.sin(angle)))
    start=len(g.vertices)
    g.prism(outline,.012,"Moss" if index%3==0 else "Reed")
    for i in range(start,len(g.vertices)):
        vx,vy,vz=g.vertices[i]
        g.vertices[i]=(vx,vy,vz+.006)
for idx in [0,4]:
    x,y,r=pads[idx]
    centre=Vector((x,y,.028))
    for j in range(5):
        a=j*math.tau/5
        out=Vector((math.cos(a),math.sin(a),0))
        cross=Vector((-math.sin(a),math.cos(a),0))
        verts=[centre+out*.02-cross*.042, centre+out*.02+cross*.042,
               centre+out*.17+Vector((0,0,.07)),centre+out*.065+Vector((0,0,.025))]
        g.add(verts,[(0,1,2),(0,3,1),(1,3,2),(2,3,0)],"PaintCream")
    g.add([centre+Vector(v) for v in [(-.035,0,0),(.035,0,0),(0,-.035,0),(0,.035,0),(0,0,.055),(0,0,-.013)]],
          [(0,2,4),(2,1,4),(1,3,4),(3,0,4),(2,0,5),(1,2,5),(3,1,5),(0,3,5)],"PaintOrange")
g.fit()
REPORTS=[b3.finish(g,"SM_LF_LilyPads",300,notes="Eight closed 12mm flat pads with V notches, two five-petal cream flowers; no team colours.",pad_count=8,flower_count=2)]
