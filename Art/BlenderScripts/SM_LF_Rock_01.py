"""B02: original small shoreline cobble, 1.15 x 0.9 x 0.72 m."""
import sys, importlib, math, random
sys.path.insert(0, r"D:\Projects\LakeFight\Art\BlenderScripts")
import lf_common as lf
importlib.reload(lf)
import lf_b02 as b
importlib.reload(b)

rng = random.Random(1101)
g = b.Geometry()
rings = []
for level, (radius, z, shift) in enumerate([(.72, 0, -.08), (1, .40, 0), (.51, 1, .12)]):
    ring = []
    for i in range(7):
        angle = i * math.tau / 7 + .11 * level + rng.uniform(-.05, .05)
        r = radius * rng.uniform(.85, 1.12)
        ring.append((math.cos(angle) * r + shift, math.sin(angle) * r,
                     z + (rng.uniform(-.08, .08) if level else 0)))
    rings.append(ring)
g.loft(rings, "Rock")
g.fit((1.15, .90, .72))
REPORTS = [b.finish(g, "SM_LF_Rock_01", 300, notes="Small rounded faceted shoreline cobble; fixed seed 1101.")]
