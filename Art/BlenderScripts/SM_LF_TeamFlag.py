"""B02.1: four dock team flags. Each FBX contains pole and separate flag.

The waved flag is a closed 1 cm shell, with a cream hoist and team field.
Both object origins are at the pole base for consistent dock attachment.
"""
import sys, importlib, math
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

REPORTS = []
for colour in ["Red", "Blue", "Green", "Yellow"]:
    name = "SM_LF_TeamFlag_" + colour
    pole, flag = b.Geometry(), b.Geometry()
    pole.box((0, 0, .025), (.22, .22, .05), "WoodDark", bevel=.009)
    pole.cylinder((0, 0, .03), (0, 0, 2.50), .035, "WoodDark", sides=8, end_radius=.025)
    for z in [1.85, 2.43]:
        pole.cylinder((0, 0, z - .015), (0, 0, z + .015), .039, "Metal", sides=8)
    pole.loft([[(r * math.cos(i * math.tau / 8), r * math.sin(i * math.tau / 8), z)
                for i in range(8)] for z, r in [(2.48, .024), (2.53, .044), (2.58, .022)]], "Metal")
    rings = []
    for i, wave in enumerate([0, .05, .015, -.055, -.015, .025]):
        x = .045 + .22 * i
        top = 2.45 - .035 * math.sin(i * math.pi / 5)
        rings.append([(x, wave - .005, top - .62), (x, wave + .005, top - .62),
                      (x, wave + .005, top), (x, wave - .005, top)])
    materials = ["PaintCream"] * 4 + ["Paint" + colour] * 16 + ["PaintCream", "Paint" + colour]
    flag.loft(rings, materials)
    REPORTS.append(b.finish_parts([(name, pole), (name + "_Flag", flag)], name, 300,
                                  notes="2.58 m dock flagpole and separate 1.10 x 0.62 m waved, solid flag; cream hoist plus team field; both origins at pole base."))
