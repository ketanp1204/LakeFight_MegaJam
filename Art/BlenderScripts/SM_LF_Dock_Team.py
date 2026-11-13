"""B05 original dock plus neutral flag tray and three closed banking crates.

Original 4x2m B02 dock geometry retained. Three closed crates use dark bevel
frames rather than dense slat geometry, keeping the whole prop under 1k tris.
Origin: deck walking surface centre, Z=0. No flag or team colour exported.
"""
import sys,importlib,math
sys.path.insert(0,r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf;importlib.reload(lf)
import lf_b05 as b5;importlib.reload(b5)
from mathutils import Matrix
g=b5.b.Geometry()
width=(4-11*.015)/12
for i in range(12):
    x=-2+width/2+i*(width+.015)
    g.box((x,0,-.06),(width,2,.12),"Wood",bevel=.008)
for x in [-1.82,1.82]:
    for y in [-.82,.82]:
        g.box((x,y,-.425),(.18,.18,2.15),"WoodDark",bevel=.015)
        g.beam((x,y,-.85),(x*.59,y,-.21),.075,.085)
for y in [-.66,0,.66]:g.box((0,y,-.21),(3.94,.12,.18),"WoodDark")
# Closed wooden fish-banking crates; dark bevel faces form the heavy edge frame.
crate_materials=["Wood"]*6+["WoodDark"]*20
for centre,yaw in [((.96,-.23,.15),0),((.96,.23,.15),0),((.98,0,.45),math.radians(12))]:
    g.box(centre,(.60,.40,.30),crate_materials,bevel=.025,rotation=Matrix.Rotation(yaw,3,"Z"))
# End-post mounted tray fits the existing flag's .22m square base with clearance.
g.box((1.82,1.08,.685),(.28,.28,.03),"Metal")
# Overlap the closed rim pieces/floor by 3mm at joints, avoiding coincident
# four-face edges if an importer welds same-position vertices across parts.
for x in [1.69,1.95]:g.box((x,1.08,.717),(.02,.28,.04),"Metal")
for y in [.95,1.21]:g.box((1.82,y,.717),(.246,.02,.04),"Metal")
g.beam((1.82,.82,.46),(1.82,1.08,.67),.07,.07,"Metal")
REPORTS=[b5.finish(g,"SM_LF_Dock_Team",1000,
    "Original 4x2m dock plus three closed framed banking crates and a post-mounted neutral metal flag-base tray. Tray accepts the separate existing TeamFlag base; no flag or team-coloured material in this export.",
    origin="walking surface centre; deck Z=0",base_dock_tris=788,crate_count=3,
    crate_dimensions_m=[.6,.4,.3],flag_holder=dict(location_m=[1.82,1.08,.70],rotation_deg=[0,0,0],scale=[1,1,1],
    inner_tray_size_m=[.24,.24],flag_base_size_m=[.22,.22],clearance_per_side_m=.01,
    instructions="Place existing SM_LF_TeamFlag_<Colour> origin at this local transform. No UE socket was created by this Blender-only task."))]
