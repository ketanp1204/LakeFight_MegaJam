"""B02: 4 x 2 m modular dock; origin at top walking-surface centre."""
import sys, importlib
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

g = b.Geometry()
width = (4 - 11 * .015) / 12
for i in range(12):
    x = -2 + width / 2 + i * (width + .015)
    g.box((x, 0, -.06), (width, 2, .12), "Wood", bevel=.008)
for x in [-1.82, 1.82]:
    for y in [-.82, .82]:
        g.box((x, y, -.425), (.18, .18, 2.15), "WoodDark", bevel=.015)
        g.beam((x, y, -.85), (x * .59, y, -.21), .075, .085)
for y in [-.66, 0, .66]:
    g.box((0, y, -.21), (3.94, .12, .18), "WoodDark")
REPORTS = [b.finish(g, "SM_LF_Dock", 800, origin="top surface centre; deck Z=0",
                    notes="4 x 2 m deck; 12 planks, four posts, three bearers, four diagonal braces. Posts extend -1.5 to +0.65 m.")]
