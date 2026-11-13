"""B04 four chunky puffy rigid life vests, spine_03 aligned socket origin."""
import sys,importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b04 as b4;importlib.reload(b4)
REPORTS=[]
for team in b4.TEAMS:
    g=b4.b.Geometry()
    paint="Paint"+team
    for y in [-.12,.12]:
        for z in [-.145,.045]:
            g.box((.155,y,z),(.17,.185,.18),paint,bevel=.027)
    g.box((-.152,0,-.035),(.105,.39,.38),paint,bevel=.025)
    for sign in [-1,1]:
        g.box((.005,sign*.218,-.07),(.24,.075,.30),paint,bevel=.022)
        g.beam((.13,sign*.145,.12),(-.14,sign*.145,.145),.075,.065,paint)
    b4.belt_rect(g,.248,.272,-.155,.040,"PaintCream",cx=.014)
    g.box((.272,0,-.135),(.026,.055,.056),"Black",bevel=.004)
    REPORTS.append(b4.finish(g,"SM_LF_LifeVest_"+team,"spine_03",
        "Four bevelled front flotation pillows, puffy back and sides, open neck/arm spaces, cream wrap strap and black buckle. Static rigid attachment; running/arm clearance requires human check.",
        "Puff centre approximately +11.625cm X, -3.75cm Z from spine_03; front opens along centre Y=0. Proxy spine_03 at 101.25cm above soles.",team))
