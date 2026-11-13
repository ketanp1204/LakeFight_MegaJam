"""B04 static 1.59m fishing rod for 0.75-scale Manny; hand_r grip pivot."""
import sys,importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b04 as b4;importlib.reload(b4)
g=b4.b.Geometry()
g.cylinder((0,0,-.17),(0,0,.16),.023,"WoodDark",sides=8)
b4.tube_path(g,[(0,0,.13),(0,0,.75),(.05,0,1.35),(.22,0,1.95)],[.013,.011,.007,.004],"Wood",sides=6)
g.beam((0,-.01,.035),(-.05,-.025,.035),.017,.018,"Metal")
g.cylinder((-.05,-.015,.035),(-.05,-.080,.035),.047,"Metal",sides=8)
g.cylinder((-.05,-.078,.035),(-.05,-.088,.035),.031,"Rope",sides=8)
g.beam((-.05,-.092,.035),(-.092,-.092,.005),.009,.009,"Metal")
g.cylinder((-.092,-.09,.005),(-.092,-.125,.005),.012,"WoodDark",sides=6)
for x,z in [(0,.55),(.043,1.25),(.217,1.91)]:
    g.torus((x+.023,0,z),.017,.003,"Metal",steps=8,tube_steps=3,plane="XY")
g.cylinder((.24,0,1.95),(.032,0,.55),.0018,"Black",sides=3)
REPORTS=[b4.finish(g,"SM_LF_FishingRod","hand_r",
    "Tapered wood shaft, dark grip, solid reel/crank, three guide eyes and static black fishing line. No animation/line simulation.",
    "Origin at middle of grip; butt Z=-12.75cm, rod tip Z=+146.25cm. Align socket local +Z along rod; review holds it 25deg forward.")]
