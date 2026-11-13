"""B04 enormous WoodDark handlebar moustache, head attachment."""
import sys,importlib
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b04 as b4;importlib.reload(b4)
g=b4.b.Geometry()
for s in [-1,1]:
    points=[(.150,0,.074),(.161,s*.05,.060),(.163,s*.113,.075),(.156,s*.151,.107),(.150,s*.136,.137)]
    b4.tube_path(g,points,[.018,.027,.021,.012,.004],"WoodDark",sides=6)
REPORTS=[b4.finish(g,"SM_LF_Moustache","head",
    "Two tapered six-sided curled handlebars; WoodDark matches beard. Separate face piece for optional combination.",
    "Upper-lip centre approximately +11.25cm X,+5.55cm Z from assumed head pivot, curl span approximately 24.3cm.")]
