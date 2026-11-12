"""B02: open slatted fish crate, 0.6 x 0.4 x 0.3 m."""
import sys, importlib
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

g = b.Geometry()
for i in range(4):
    g.box((0, (i - 1.5) * .084, .015), (.55, .077, .03), "Wood")
for z in [.075, .15, .225]:
    for y in [-.1875, .1875]:
        g.box((0, y, z), (.6, .025, .055), "Wood")
    for x in [-.2875, .2875]:
        g.box((x, 0, z), (.025, .35, .055), "Wood")
for x in [-.271, .271]:
    for y in [-.171, .171]:
        g.box((x, y, .165), (.032, .032, .27), "WoodDark")
for y in [-.1825, .1825]:
    g.box((0, y, .285), (.6, .035, .03), "WoodDark")
for x in [-.2825, .2825]:
    g.box((x, 0, .285), (.035, .33, .03), "WoodDark")
REPORTS = [b.finish(g, "SM_LF_Crate", 300, notes="Open top and genuine slat gaps; no lid or hidden solid fill.")]
