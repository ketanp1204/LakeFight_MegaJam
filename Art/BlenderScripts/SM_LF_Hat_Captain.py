"""B04 cream/black captain cap, single leader variant, hollow underside."""
import sys,importlib,math
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b04 as b4;importlib.reload(b4)
g=b4.b.Geometry()
profile=[(.120,.105,.302),(.147,.128,.280),(.149,.130,.205),(.132,.112,.150),(.124,.104,.150),(.112,.096,.282)]
g.loft([b4.ellipse_ring(rx,ry,z) for rx,ry,z in profile],"PaintCream")
b4.belt(g,.135,.115,.150,.036,"Black")
outline=[(.065,-.112),(.15,-.132),(.225,-.09),(.245,0),(.225,.09),(.15,.132),(.065,.112)]
start=len(g.vertices)
g.prism(outline,.016,"Black")
for i in range(start,len(g.vertices)):
    x,y,z=g.vertices[i];g.vertices[i]=(x,y,z+.145-.06*(x-.065))
# Original geometric anchor badge in black on cream, facing +X.
start=len(g.vertices)
g.torus((0,0,0),.015,.004,"Black",steps=8,tube_steps=3,plane="XY")
for i in range(start,len(g.vertices)):
    x,y,z=g.vertices[i];g.vertices[i]=(.153+z,y,.253+x)
g.box((.155,0,.222),(.008,.009,.038),"Black")
g.beam((.155,0,.210),(.155,-.024,.226),.008,.008,"Black")
g.beam((.155,0,.210),(.155,.024,.226),.008,.008,"Black")
REPORTS=[b4.finish(g,"SM_LF_Hat_Captain","head",
    "Cream puffy crown, black band/forward peak and original geometric anchor badge. Hollow head cavity.",
    "Band lower edge +11.25cm Z and crown top +22.65cm Z from assumed head pivot; visor +X.")]
