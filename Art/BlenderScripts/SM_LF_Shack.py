"""B02: original 4 x 3 m shore shack with genuine door/window openings.

No glass, closed door, backing wall or solid fill over either front opening.
The 4.4 x 3.4 m pitched roof overhangs the 4 x 3 m wall footprint.
"""
import sys, importlib, math
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)
from mathutils import Vector
from mathutils.bvhtree import BVHTree
import bpy

g = b.Geometry()
EAVE, RIDGE = 2.4, 3.2


def panel(x0, x1, z0, z1, y):
    n = math.ceil((x1 - x0) / .24)
    w = (x1 - x0) / n
    for i in range(n):
        # Flush but individually shaded siding; no texture or fake opening decals.
        g.box((x0 + (i + .5) * w, y, (z0 + z1) / 2),
              (w, .08, z1 - z0), "Wood")


for x0, x1, z0, z1 in [(-2, -1.25, .1, EAVE), (-1.25, -.30, 2.15, EAVE),
                        (-.30, .45, .1, EAVE), (.45, 1.35, .1, 1.05),
                        (.45, 1.35, 1.85, EAVE), (1.35, 2, .1, EAVE)]:
    panel(x0, x1, z0, z1, -1.46)
panel(-2, 2, .1, EAVE, 1.46)

# Individually fitted end boards continue into both triangular gables.
for x in [-1.96, 1.96]:
    for i in range(12):
        ya, yb = -1.5 + i * .25, -1.5 + (i + 1) * .25
        za, zb = (EAVE + (RIDGE - EAVE) * (1 - abs(y) / 1.5) for y in [ya, yb])
        g.loft([[(x - .04, ya, .1), (x + .04, ya, .1),
                 (x + .04, yb, .1), (x - .04, yb, .1)],
                [(x - .04, ya, za), (x + .04, ya, za),
                 (x + .04, yb, zb), (x - .04, yb, zb)]], "Wood")

for i in range(8):
    g.box((-1.75 + i * .5, 0, .05), (.494, 3, .1), "WoodDark")
for x in [-1.95, 1.95]:
    for y in [-1.45, 1.45]:
        g.box((x, y, 1.25), (.14, .14, 2.3), "WoodDark")

# Door: 0.95 m wide, 2.05 m clear above the floor. Cream trim sits outside the hole.
for x in [-1.295, -.255]:
    g.box((x, -1.515, 1.1475), (.09, .09, 2.095), "PaintCream")
g.box((-.775, -1.515, 2.195), (1.13, .09, .09), "PaintCream")
# Window: 0.9 x 0.8 m clear opening, with a projecting sill.
for x in [.405, 1.395]:
    g.box((x, -1.515, 1.45), (.09, .09, .98), "PaintCream")
for z in [1.005, 1.895]:
    g.box((.9, -1.515, z), (1.08, .09, .09), "PaintCream")
g.box((.9, -1.57, 1.005), (1.16, .24, .065), "WoodDark")

def roof_z(y):
    return RIDGE - abs(y) * (RIDGE - EAVE) / 1.5

for sign in [-1, 1]:
    quad = [(-2.2, 0, roof_z(0)), (2.2, 0, roof_z(0)),
            (2.2, sign * 1.7, roof_z(1.7)), (-2.2, sign * 1.7, roof_z(1.7))]
    g.loft([quad, [(x, y, z + .075) for x, y, z in quad]], "Moss")
    for x in [-2.2, 2.2]:
        g.beam((x, 0, RIDGE + .025), (x, sign * 1.7, roof_z(1.7) + .025),
               .10, .10, "PaintCream")
    for x in [-1.65, -.55, .55, 1.65]:
        g.beam((x, 0, RIDGE + .085), (x, sign * 1.7, roof_z(1.7) + .085),
               .022, .016, "Moss")
g.box((0, 0, RIDGE + .075), (4.43, .10, .055), "WoodDark")

REPORTS = [b.finish(g, "SM_LF_Shack", 1500,
                    notes="4 x 3 m walls, overhanging Moss pitched roof, 0.95 x 2.05 m clear doorway and 0.9 x 0.8 m window. Both openings ray-tested; no glass or door panel.")]
tree = BVHTree.FromObject(bpy.data.objects["SM_LF_Shack"], bpy.context.evaluated_depsgraph_get())
for x, z in [(-.775, 1.1), (.9, 1.45)]:
    hit = tree.ray_cast(Vector((x, -1.9, z)), Vector((0, 1, 0)), .7)
    assert hit[0] is None, ("Opening blocked", x, z)
assert tree.ray_cast(Vector((-1.65, -1.9, 1.2)), Vector((0, 1, 0)), .7)[0] is not None
print("[B02] Shack front door/window ray tests PASS; adjacent wall control ray hits.")
