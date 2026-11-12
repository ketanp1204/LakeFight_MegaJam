"""B02: original broad shoreline slab, 3 x 2.1 x 1.1 m."""
import sys, importlib, math, random
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

rng = random.Random(1103)
g = b.Geometry()
rings = []
for level, (radius, z, shift) in enumerate([(.83, 0, 0), (1, .22, -.03), (.92, .76, .06), (.69, 1, .09)]):
    ring = []
    for i in range(9):
        angle = i * math.tau / 9 - .07 * level + rng.uniform(-.035, .035)
        r = radius * rng.uniform(.88, 1.12)
        ring.append((math.cos(angle) * r + shift, math.sin(angle) * r,
                     z + (rng.uniform(-.055, .055) if level in [1, 2] else 0)))
    rings.append(ring)
g.loft(rings, "Rock")
g.fit((3, 2.1, 1.1))
REPORTS = [b.finish(g, "SM_LF_Rock_03", 300, notes="Large single slab with a planar top at 1.1 m; no dish; fixed seed 1103.")]
