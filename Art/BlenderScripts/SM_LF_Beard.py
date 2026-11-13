"""B04 oversized pointed WoodDark beard with open mouth notch; head attachment."""
import sys,importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b04 as b4;importlib.reload(b4)
g=b4.b.Geometry()
outline=[(-.108,.105),(-.060,.062),(0,.040),(.060,.062),(.108,.105),
         (.105,-.015),(.056,-.060),(0,-.115),(-.056,-.060),(-.105,-.015)]
rings=[]
for x,scale in [(.095,.96),(.165,1.),(.193,.78)]:
    rings.append([(x,y*scale,z*scale) for y,z in outline])
g.loft(rings,"WoodDark")
for s in [-1,1]:
    g.beam((.075,s*.092,.11),(.105,s*.098,.005),.035,.042,"WoodDark")
REPORTS=[b4.finish(g,"SM_LF_Beard","head",
    "Chunky pointed beard with raised cheek tufts, two sideburn blocks and a V mouth opening; WoodDark only.",
    "Front surface X=7.125..14.475cm, lower tip Z=-8.625cm, cheek tips Z=+8.25cm from assumed head pivot.")]
