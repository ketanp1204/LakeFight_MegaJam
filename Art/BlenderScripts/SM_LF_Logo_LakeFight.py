"""B05 LAKE FIGHT: original geometry-only lettering, chipped board, rear paddle."""
import sys,importlib,math
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b05 as b5;importlib.reload(b5)
g=b5.b.Geometry()
board=[(-2.5,-.35),(-2.43,-.50),(-1.83,-.47),(-1.77,-.50),(.48,-.48),
       (1.75,-.52),(2.39,-.46),(2.5,-.32),(2.46,.37),(2.37,.50),
       (1.28,.47),(1.23,.51),(-.65,.48),(-2.40,.46)]
g.prism(board,.18,"WoodDark",plane="XZ")
text_width=b5.lettering(g,"LAKE FIGHT",.36,.62,.075,.255,0,-.095,.10,202611130501)
# An original little paddle behind the sign: plane XZ, pitched across the board.
p=b5.b.Geometry()
p.box((.035,0,0),(.07,.18,.065),"WoodDark",bevel=.009)
p.cylinder((.055,0,0),(1.145,0,0),.025,"Wood",sides=8)
p.prism([(1.055,-.034),(1.19,-.13),(1.47,-.16),(1.60,-.10),
         (1.60,.10),(1.47,.16),(1.19,.13),(1.055,.034)],.035,"Wood")
for x in [.93,.97]:p.cylinder((x,0,0),(x+.018,0,0),.030,"WoodDark",sides=8)
angle=math.radians(35);s=1.9
vertices=[((x-.8)*s*math.cos(angle)-y*s*math.sin(angle),.19+z*s,
           (x-.8)*s*math.sin(angle)+y*s*math.cos(angle)) for x,y,z in p.vertices]
g.add(vertices,p.faces,p.materials)
REPORTS=[b5.finish(g,"SM_LF_Logo_LakeFight",5000,
    "Original irregular block-cell LAKE FIGHT lettering extruded 10cm, chipped WoodDark sign and one diagonal rear paddle. No font object, font file, texture or external logo.",
    origin="main board centre; X horizontal, Z up, front -Y",title="LAKE FIGHT",
    lettering_source="explicit original GLYPHS cell masks in lf_b05.py",letter_depth_m=.10,text_width_m=text_width,
    directors_cut_companion="SM_LF_Logo_DirectorsCut; same origin and orientation")]
