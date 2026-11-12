"""B02: original leaning shoreline boulder, 2.1 x 1.4 x 1.2 m."""
import sys, importlib, math, random
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

rng = random.Random(1102)
g = b.Geometry()
rings = []
for level, (radius, z, shift) in enumerate([(.80, 0, -.18), (1, .23, -.12), (.81, .73, .14), (.40, 1, .30)]):
    ring = []
    for i in range(8):
        angle = i * math.tau / 8 + .09 * level + rng.uniform(-.04, .04)
        r = radius * rng.uniform(.80, 1.10)
        ring.append((math.cos(angle) * r + shift, math.sin(angle) * r,
                     z + (rng.uniform(-.075, .075) if level else 0)))
    rings.append(ring)
g.loft(rings, "Rock")
g.fit((2.1, 1.4, 1.2))
REPORTS = [b.finish(g, "SM_LF_Rock_02", 300, notes="Medium leaning boulder with broad lower shoulder; fixed seed 1102.")]
