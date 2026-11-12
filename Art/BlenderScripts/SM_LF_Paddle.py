"""B02: original chunky 1.6 m paddle; origin at grip, blade along +X."""
import sys, importlib, math
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

g = b.Geometry()
g.box((.035, 0, 0), (.07, .18, .065), "WoodDark", bevel=.009)
g.cylinder((.055, 0, 0), (1.145, 0, 0), .027, "Wood", sides=8, end_radius=.034)
blade = [(1.055, -.034), (1.19, -.115), (1.47, -.145), (1.60, -.10),
         (1.60, .10), (1.47, .145), (1.19, .115), (1.055, .034)]
g.prism(blade, .042, "Wood")
for x in [.93, .97]:
    g.cylinder((x, 0, 0), (x + .018, 0, 0), .031, "WoodDark", sides=8)
REPORTS = [b.finish(g, "SM_LF_Paddle", 300, origin="grip; blade +X",
                    notes="Length exactly 1.6 m from grip end to blade tip.")]
